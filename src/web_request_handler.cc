/*  web_request_handler.cc - this file is part of MediaTomb.
                                                                                
    Copyright (C) 2005 Gena Batyan <bgeradz@deadlock.dhs.org>,
                       Sergey Bostandzhyan <jin@deadlock.dhs.org>
                                                                                
    MediaTomb is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
                                                                                
    MediaTomb is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
                                                                                
    You should have received a copy of the GNU General Public License
    along with MediaTomb; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "autoconfig.h"
#include <time.h>
#include "config_manager.h"
#include "mem_io_handler.h"
#include "web_request_handler.h"
#include "config_manager.h"
#include "web/pages.h"
#include "session_manager.h"
#include "tools.h"
#include "hash.h"

using namespace zmm;
using namespace mxml;

#define DEFAULT_PAGE_BUFFER_SIZE 4096

//static JSObject *shared_global_object = NULL;

//static Ref<DSOHash<WebScript> > script_hash(new DSOHash<WebScript>(37));

WebRequestHandler::WebRequestHandler() : RequestHandler()
{
    pagename = nil;
    plainXML = false;
}

String WebRequestHandler::param(String name)
{
    return params->get(name);
}

/*String WebRequestHandler::buildScriptPath(String filename)
{
    String scriptRoot = ConfigManager::getInstance()->getOption(_("/server/webroot"));
    return String(scriptRoot + "/jssp/") + filename;
}*/

void WebRequestHandler::check_request()
{
    // we have a minimum set of parameters that are "must have"

    // check if the session parameter was supplied and if we have
    // a session with that id
    String sid = param(_("sid"));
    if (sid == nil)
    {
        throw SessionException(_("no session id given"));
    }
    if (SessionManager::getInstance()->getSession(sid) == nil)
    {
        throw SessionException(_("invalid session id"));
    }
}

String WebRequestHandler::renderXMLHeader()
{
    return _("<?xml version=\"1.0\" encoding=\"") +
            DEFAULT_INTERNAL_CHARSET +"\"?>\n";
}

void WebRequestHandler::get_info(IN const char *filename, OUT struct File_Info *info)
{
    info->file_length = -1; // length is unknown
    info->last_modified = time(NULL);
    info->is_directory = 0;
    info->is_readable = 1;

    String contentType;

    if (plainXML)
        contentType = _(MIMETYPE_XML) + "; charset=" + DEFAULT_INTERNAL_CHARSET;
    else
        contentType = _("text/html; charset=") +
                      DEFAULT_INTERNAL_CHARSET;
    
    info->content_type = ixmlCloneDOMString(contentType.c_str());
    
}

Ref<IOHandler> WebRequestHandler::open(Ref<Dictionary> params, IN enum UpnpOpenFileMode mode)
{
    this->params = params;

    root = Ref<Element>(new Element(_("root")));
    out = Ref<StringBuffer>(new StringBuffer());

    if (pagename == nil)
        pagename = _("index");

    String output;
    // processing page, creating output
    try
    {
        process();
        // log_debug(("RENDERING %s: \n%s\n", pagename, root->print().c_str()));
        if (plainXML)
        {
            output = renderXMLHeader() + root->print();
        }
/*        else
        {
            String p(pagename);
            String scriptPath = buildScriptPath(pagename + ".jssp");
            hash_slot_t hash_slot;
            Ref<WebScript> script = script_hash->get(p, &hash_slot);
            if (script == nil)
            {
//                log_debug(("creating script for page %s\n", pagename));
                script = Ref<WebScript>(new WebScript(Runtime::getInstance(),
                                        scriptPath, shared_global_object));
                if (! shared_global_object)
                    shared_global_object = script->getGlobalObject();
                
                script_hash->put(p, hash_slot, script);
            }
//            else
//                log_debug(("cashed script for page %s\n", pagename));
            script->setSessionID(param(_("sid")));
            output = script->process(root);
        }
        */
    }
    catch (SessionException se)
    {
        //String url = _("/content/interface?req_type=login&slt=") +
        //                    generate_random_id();
        
        String url = _("/");                    
        if (plainXML)
        {
            root->appendTextChild(_("redirect"), url);
            output = renderXMLHeader() + root->print();
        }
        else
        {
            output = _(
            "<html><head>"
            "<script>top.location.href = '")+ url +"';</script>"
            "</head><body></body></html>";
        }
    }
    catch (Exception e)
    {
        e.printStackTrace();
        // Ref<Dictionary> par(new Dictionary());
        // par->put("message", e.getMessage());
        // output = subrequest("error", par);
        String errmsg = _("JSSP ERROR (page ") + pagename +") : " + e.getMessage();
        if (plainXML)
        {
            root->appendTextChild(_("error"), errmsg);
            output = renderXMLHeader() + root->print();
        }
        else
        {
            output = errmsg;
        }
    }
    root = nil;

    Ref<MemIOHandler> io_handler(new MemIOHandler(output));
    io_handler->open(mode);
    return RefCast(io_handler, IOHandler);
}

Ref<IOHandler> WebRequestHandler::open(IN const char *filename,
                                       IN enum UpnpOpenFileMode mode)
{
    this->filename = String((char *)filename);
    this->mode = mode;

    String path, parameters;
    split_url(filename, path, parameters);

    Ref<Dictionary> params = Ref<Dictionary>(new Dictionary());
    params->decode(parameters);
    return open(params, mode);
}

String WebRequestHandler::subrequest(String req_type, Ref<Dictionary> params)
{
    Ref<WebRequestHandler> subhandler(create_web_request_handler(req_type));
    Ref<IOHandler> io_handler = subhandler->open(params, mode);
    
    Ref<StringBuffer> buffer(new StringBuffer(DEFAULT_PAGE_BUFFER_SIZE));
    char *buf = (char *)MALLOC(DEFAULT_PAGE_BUFFER_SIZE * sizeof(char));
    while (true)
    {
        int bytesRead = io_handler->read(buf, DEFAULT_PAGE_BUFFER_SIZE);
        if (bytesRead <= 0)
            break;
        *buffer << String(buf, bytesRead);
    }
    free(buf);
    return buffer->toString();
}


// this method should be overridden
void WebRequestHandler::process()
{
}


