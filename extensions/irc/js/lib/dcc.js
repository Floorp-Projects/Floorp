/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *  
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is JSIRC Library
 *
 * The Initial Developer of the Original Code is New Dimensions Consulting,
 * Inc. Portions created by New Dimensions Consulting, Inc. are
 * Copyright (C) 1999 New Dimenstions Consulting, Inc. All
 * Rights Reserved.
 *
 * Contributor(s):
 *  Robert Ginda, rginda@ndcico.com, original author
 *
 * JavaScript utility functions.
 *
 * 1999-08-15 rginda@ndcico.com           v1.0
 */

function CDCCChat (ep, host, port)
{
    
    this.READ_TIMEOUT = 50;

    this.eventPump = ep;
    this.host = host;
    this.port = port;
    this.connection = new CBSConnection();

    this.savedLine = "";

}

CDCCChat.prototype.connect =
function dchat_connect (host, port)
{
    
    if (typeof host != "undefined") this.host = host;
    if (typeof port != "undefined") this.port = port;

    if (!this.connection.connect (this.host, this.port, (void 0), true))
        return false;

    this.eventPump.addEvent (new CEvent ("dcc-chat", "poll", this, "onPoll"));

    return true;
    
}

CDCCChat.prototype.onPoll =
function dchat_poll (e)
{
    var line = "";
    
    try
    {    
        line = this.connection.readData (this.READ_TIMEOUT);
    }
    catch (ex)
    {
        if (typeof (ex) != "undefined")        
        {
            this.connection.disconnect();
            var e = new CEvent ("dcc-chat", "close", this, "onClose");
            e.reason = "read-error";
            e.exception = ex;
            this.eventPump.addEvent (e);
            return false;
        }
        else
            line = "";
    }

    this.eventPump.addEvent (new CEvent ("dcc-chat", "poll", this, "onPoll"));
    
    if (line == "")
        return false;
    
    var incomplete = (line[line.length] != '\n');
    var lines = line.split("\n");

    if (this.savedLine)
    {
        lines[0] = this.savedLine + lines[0];
        this.savedLine = "";
    }
    
    if (incomplete)
        this.savedLine = lines.pop();
    
    for (i in lines)
    {
        var ev = new CEvent("dcc-chat", "rawdata", this, "onRawData");
        ev.data = lines[i];
        ev.replyTo = this;
        this.eventPump.addEvent (ev);
    }
    
    return true;
    
}

CDCCChat.prototype.say =
function dchat_say (msg)
{

    this.connection.sendData (msg + "\n");

}
