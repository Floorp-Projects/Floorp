/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is JSIRC Library.
 *
 * The Initial Developer of the Original Code is
 * New Dimensions Consulting, Inc.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Robert Ginda, rginda@ndcico.com, original author
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

function CHTTPDoc (server, path)
{
    
    this.GET_TIMEOUT = 2 * 60; /* 2 minute timeout on gets */
    this.state = "new";
    this.server = server;
    this.path = path;
    
}

CHTTPDoc.prototype.get =
function http_get (ep)
{

    this.connection = new CBSConnection();
    if (!this.connection.connect (this.server, 80, (void 0), true))
    {
        this.state = "connect-error";
        var e = new CEvent ("httpdoc", "complete", this, "onComplete");
        this.eventPump.addEvent (e);
        return false;
    }

    this.eventPump = ep;
    this.state = "opened";
    this.data = "";
    this.duration = 0;

    var e = new CEvent ("httpdoc", "poll", this, "onPoll");
    this.eventPump.addEvent (e);

    this.connection.sendData ("GET " + this.path + "\n");
    this.startDate = new Date();
    
    return true;
    
}

CHTTPDoc.prototype.onPoll =
function http_poll (e)
{
    var line = "";
    var ex, c;
    var need_more = false;

    if (this.duration < this.GET_TIMEOUT)
        try
        {    
            line = this.connection.readData (50);
            need_more = true;
        }
            catch (ex)
        {
            if (typeof (ex) == "undefined")
            {
                line = "";
                need_more = true;
            }
            else
            {
                dd ("** Caught exception: '" + ex + "' while receiving " +
                    this.server + this.path);
                this.state = "read-error";
            }
        }
    else
        this.state = "receive-timeout";

    switch (this.state)
    {
        case "opened":
        case "receive-header":
            if (this.state == "opened")
                this.headers = "";
            
            c = line.search(/\<html\>/i);
            if (c != -1)
            {
                this.data = line.substr (c, line.length);
                this.state = "receive-data";
                line = line.substr (0, c);

                c = this.data.search(/\<\/html\>/i);
                if (c != -1)
                {
                    this.docType = stringTrim(this.docType);    
                    this.state = "complete";
                    need_more = false;
                }
                
            }

            this.headers += line;
            c = this.headers.search (/\<\!doctype/i);
            if (c != -1)
            {
                this.docType = this.headers.substr (c, line.length);
                if (this.state == "opened")
                    this.state = "receive-doctype";
                this.headers = this.headers.substr (0, c);
            }
            
            if (this.state == "opened")
                this.state = "receive-header";
            break;

        case "receive-doctype":
            var c = line.search (/\<html\>/i);
            if (c != -1)
            {
                this.docType = stringTrim(this.docType);    
                this.data = line.substr (c, line.length);
                this.state = "receive-data";
                line = line.substr (0, c);
            }

            this.docType += line;
            break;

        case "receive-data":
            this.data += line;
            var c = this.data.search(/\<\/html\>/i);
            if (c != -1)
                this.state = "complete";
            break;

        case "read-error":
        case "receive-timeout":
            break;
            
        default:
            dd ("** INVALID STATE in HTTPDoc object (" + this.state + ") **");
            need_more = false;
            this.state = "error";
            break;

    }
    
    if ((this.state != "complete") && (need_more))
        var e = new CEvent ("httpdoc", "poll", this, "onPoll");
    else
    {
        this.connection.disconnect();
        if (this.data)
        {
            var m = this.data.match(/\<title\>(.*)\<\/title\>/i);
            if (m != null)
                this.title = m[1];
            else
                this.title = "";
        }
        var e = new CEvent ("httpdoc", "complete", this, "onComplete");
    }
    
    this.eventPump.addEvent (e);
    this.duration = (new Date() - this.startDate) / 1000;

    return true;
    
}

CHTTPDoc.prototype.onComplete =
function http_complete(e)
{

    return true;

}



