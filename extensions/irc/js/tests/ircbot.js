/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/ 
 * 
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License. 
 *
 * The Original Code is JSIRC Sample bot
 *
 * The Initial Developer of the Original Code is New Dimensions Consulting,
 * Inc. Portions created by New Dimensions Consulting, Inc. Copyright (C) 1999
 * New Dimenstions Consulting, Inc. All Rights Reserved.
 *
 *
 * Contributor(s):
 *  Robert Ginda, rginda@ndcico.com, original author
 *
 * requires utils.js, events.js, irc.js, http.js
 * see also irctest.js, which takes care of loading everything up
 * and kicking this off.
 *
 * depends on utils.js, events.js, connection.js, http.js, and irc.js
 *
 * Sample client for the irc library.
 */

var LIB_PATH = "../lib/";

bot = new Object();
bot.prefix = "!js ";

function loadDeps()
{

    load (LIB_PATH + "utils.js");
    load (LIB_PATH + "events.js");
    load (LIB_PATH + "connection.js");
    load (LIB_PATH + "http.js");
    load (LIB_PATH + "dcc.js");
    load (LIB_PATH + "irc.js");

	if (!connection_init(LIB_PATH))
	  return false;
    
	return true;

}

function initStatic()
{
    
  CIRCNetwork.prototype.INITIAL_NICK = "jsbot";
  CIRCNetwork.prototype.INITIAL_NAME = "XPJSBot";
  CIRCNetwork.prototype.INITIAL_DESC = "XPCOM Javascript bot";
  CIRCNetwork.prototype.INITIAL_CHANNEL = "#jsbot";

  CIRCNetwork.prototype.stayingPower = true; 

  CIRCChannel.prototype.onPrivmsg = my_chan_privmsg;
  CIRCUser.prototype.onDCCChat = my_user_dccchat;
  CDCCChat.prototype.onRawData = my_dccchat_rawdata;
    
}

/*
 * One time initilization stuff
 */     
function init(obj)
{
    
  obj.networks = new Object();
  obj.eventPump = new CEventPump (100);

  obj.networks["hybridnet"] =
	new CIRCNetwork ("hybridnet", [{name: "irc.ssc.net", port: 6667}],
					 obj.eventPump);
  obj.networks["linuxnet"] =
	new CIRCNetwork ("linuxnet", [{name: "irc.mozilla.org", port: 6667}],
					 obj.eventPump);
  obj.networks["efnet"] =
	new CIRCNetwork ("efnet", [{name: "irc.primenet.com", port: 6667},
							   {name: "irc.cs.cmu.edu",   port: 6667}],
					 obj.eventPump);

  obj.primNet = obj.networks["efnet"];

}

/*
 * Kick off the mainloop for the first time
 */
function go()
{

    if (!loadDeps())
	  return false;
	initStatic();
    init(bot);
    if (DEBUG)
        /* hook all events EXCEPT server.poll and *.event-end types
         * (the 4th param inverts the match) */
        bot.eventPump.addHook ([{type: "poll", set: /^(server|dcc-chat)$/},
                               {type: "event-end"}], event_tracer,
                               "event-tracer", true /* negate */);
	bot.primNet.connect();
    rego();
    
	return true;

}

/*
 * If you didnt compile libjs with JS_HAS_ERROR_EXCEPTIONS, any error the
 * bot encounters will exit the mainloop and drop you back to a shell ("js>")
 * prompt.  You can continue the mainloop by executing this function.
 */
function rego()
{

    /* mainloop */
    while (bot.eventPump.queue.length > 0)
    {
        bot.eventPump.stepEvents();
        gc();
    }
	dd ("No events to process.");

	return true;

}

function loadHooks (max, obj)
{

  if (typeof obj == "undefined")
	obj = {type:"phoney-type", set:"phoney-set"};

  for (i = 0; i < max; i++)
	bot.eventPump.addEvent (obj, (void 0), "load-hook");

}

/*
 * The following my_* are attached to their proper objects in the init()
 * function.  This is because the CIRC* objects are not defined at load time
 * (they get defined when loadDeps() loads the irc library) and so connecting
 * them here would cause an error.
 */

/*
 * What to do when a privmsg is recieved on a channel
 */
function my_chan_privmsg (e)
{
    
    if (e.meat.indexOf (bot.prefix) == 0)
    {
        try
        {
		  var v = eval(e.meat.substring (bot.prefix.length, e.meat.length));
        }
        catch (ex)
        {
		  this.say (e.user.nick + ": " + String(ex));
		  return false;
        }
        
        if (typeof (v) != "undefined")
        {						
            if (v != null)                
                v = String(v);
            else
                v = "null";
            
            var rsp = e.user.nick + ", your result is,";
            
            if (v.indexOf ("\n") != -1)
                rsp += "\n";
            else
                rsp += " ";
            
            this.say (rsp + v);
        }
    }
    
}

/*
 * What to do when a dcc chat request reaches a user object
 */
function my_user_dccchat (e)
{

    if (!e.user.canDCC)
	{
	  e.user.notice ("\01DCC REJECT CHAT chat\01");
	  return false;
	}

    var c = new CDCCChat (bot.eventPump);

    if (!c.connect (e.user.host, e.port))
	{
	  e.user.notice ("\01DCC REJECT CHAT chat\01");
	  return false;
	}

    return true;
    
}

/*
 * What to do when raw data is recieved on a dcc chat connection
 */
function my_dccchat_rawdata (e)
{

    try
    {
	  var v = eval(e.data);
    }
    catch (ex)
    {
	  this.say (String(ex));
	  return false;
    }
        
    if (typeof (v) != "undefined")
    {						
	  if (v != null)          
		v = String(v);
	  else
		v = "null";
                        
	  this.say (v);
    }
    
}

/*
 * Hook used to trace events.
 */
function event_tracer (e)
{
    var name="", data="";

    switch (e.set)
    {
        case "server":
            name = e.destObject.connection.host;
            if (e.type == "rawdata")
                data = "'" + e.data + "'";
            break;

        case "channel":
            name = e.destObject.name;
            break;
            
        case "user":
            name = e.destObject.nick;
            break;

        case "httpdoc":
            name = e.destObject.server + e.destObject.path;
            if (e.destObject.state != "complete")
                data = "state: '" + e.destObject.state + "', recieved " +
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

	if (e.level == 1)
		dd ("");

    str = "Level " + e.level + ": '" + e.type + "', " +
        e.set + name + "." + e.destMethod;
	if (data)
	  str += "\ndata   : " + data;

    dd (str);

    return true;

}

/*
 * Wrapper around CHTTPDoc to make is simpler to use
 */
function loadHTTP (host, path, onComplete)
{
    var htdoc = new CHTTPDoc (host, path);

    htdoc.onComplete = onComplete;
    htdoc.get (bot.eventPump);

    return htdoc;
    
}
