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
 */

/*
 * Hook used to trace events.
 */
function event_tracer (e)
{
    var name = "";
    var data = ("debug" in e) ? e.debug : "";
    
    switch (e.set)
    {
        case "server":
            name = e.destObject.connection.host;
            if (e.type == "rawdata")
                data = "'" + e.data + "'";
            if (e.type == "senddata")
            {
                var nextLine =
                    e.destObject.sendQueue[e.destObject.sendQueue.length - 1];
                if ("logged" in nextLine)
                    return true; /* don't print again */
                
                if (nextLine) {
                    data = "'" + nextLine.replace ("\n", "\\n") + "'";
                    nextLine.logged = true;
                }
                else
                    data = "!!! Nothing to send !!!";                
            }
            break;

        case "network":
        case "channel":
            name = e.destObject.name;
            break;

        case "user":
            name = e.destObject.nick;
            break;

        case "httpdoc":
            name = e.destObject.server + e.destObject.path;
            if (e.destObject.state != "complete")
                data = "state: '" + e.destObject.state + "', received " +
                    e.destObject.data.length;
            else
                dd ("document done:\n" + dumpObjectTree (this));
            break;

        case "dcc-chat":
            name = e.destObject.host + ":" + e.destObject.port;
            if (e.type == "rawdata")
                data = "'" + e.data + "'";
            break;

        case "client":
            if (e.type == "do-connect")
                data = "attempt: " + e.attempt + "/" +
                    e.destObject.MAX_CONNECT_ATTEMPTS;
            break;

        default:
            break;
    }

    if (name)
        name = "[" + name + "]";

    if (e.type == "info")
        data = "'" + e.msg + "'";
    
    var str = "Level " + e.level + ": '" + e.type + "', " +
        e.set + name + "." + e.destMethod;
	if (data)
	  str += "\ndata   : " + data;

    dd (str);

    return true;

}
