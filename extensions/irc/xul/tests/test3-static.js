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
 * The Original Code is JSIRC Test Client #3
 *
 * The Initial Developer of the Original Code is New Dimensions Consulting,
 * Inc. Portions created by New Dimensions Consulting, Inc. Copyright (C) 1999
 * New Dimenstions Consulting, Inc. All Rights Reserved.
 *
 *
 * Contributor(s):
 *  Robert Ginda, rginda@ndcico.com, original author
 */

var client = new Object();

client.COMMAND_CHAR = "/";
client.STEP_TIMEOUT = 500;
client.UPDATE_DELAY = 500;
client.EXPAND_HEIGHT = "200px";
client.COLLAPSE_HEIGHT = "25px";
client.TYPE = "IRCClient";
client.OP1_IMG = "g_blue_on.gif";    /* user is op image */
client.OP0_IMG = "g_blue.gif"; /* user isnt op image */
client.V1_IMG = "g_grey_on.gif";    /* user is voice image */
client.V0_IMG = "g_grey.gif"; /* user isnt voide image */
client.ACT_IMG = "green-on.gif";   /* view has activity image */
client.NACT_IMG = "green-off.gif"; /* view has no activity image */
client.CUR_IMG = "yellow-on.gif"; /* view is currently displayed */
client.PRINT_DIRECTION = -1; /*1 => new messages at bottom, -1 => at top */
                
client.name = "*client*";
client.viewsArray = new Array();
client.currentObject = client;
client.lastListType = "chan-users";

CIRCNetwork.prototype.INITIAL_NICK = "IRCMonkey";
CIRCNetwork.prototype.INITIAL_NAME = "chatzilla";
CIRCNetwork.prototype.INITIAL_DESC = "New Now Know How";
CIRCNetwork.prototype.INITIAL_CHANNEL = "";
CIRCNetwork.prototype.MAX_MESSAGES = 50;
CIRCNetwork.prototype.IGNORE_MOTD = false;

CIRCServer.prototype.READ_TIMEOUT = 0;
CIRCServer.prototype.VERSION_RPLY += "\nChatZilla test client #3";
if (jsenv.HAS_DOCUMENT)
{
    CIRCServer.prototype.VERSION_RPLY += ", running under " +
        navigator.userAgent;
}

CIRCUser.prototype.MAX_MESSAGES = 100;

CIRCChannel.prototype.MAX_MESSAGES = 200;

CIRCChanUser.prototype.MAX_MESSAGES = 100;


function initStatic()
{
    var obj;
    
    obj = document.getElementById("input");
    obj.onkeyup = onInputKeyUp;

    obj = document.getElementById("tb[*client*]");
    
    client.quickList = new CListBox(document.getElementById("quickList"));

    var saveDir = client.PRINT_DIRECTION;
    client.PRINT_DIRECTION = 1;
    client.display ("Welcome to ChatZilla...\n" +
                    "Use /attach <network-name> connect to a network.\n" +
                    "Where <network-name> is one of [" +
                    keys (client.networks) + "]\n" +
                    "More help is available with /help [<command-name>]",
                    "HELLO");
    client.PRINT_DIRECTION = saveDir;
    
    setCurrentObject (client);
    
}

function initHost(obj)
{

    client.commands = new CCommandManager();
    addCommands (obj.commands);
    
    obj.networks = new Object();
    obj.eventPump = new CEventPump (10);
    
    obj.networks["efnet"] =
	new CIRCNetwork ("efnet", [{name: "irc.primenet.com", port: 6667},
                                  {name: "irc.cs.cmu.edu",   port: 6667}],
                         obj.eventPump);
    obj.networks["linuxnet"] =
	new CIRCNetwork ("linuxnet", [{name: "irc.mozilla.org", port: 6667}],
                         obj.eventPump);
    obj.networks["hybridnet"] =
        new CIRCNetwork ("hybridnet", [{name: "irc.ssc.net", port: 6667}],
                         obj.eventPump);

    obj.primNet = obj.networks["efnet"];

    if (DEBUG)
        /* hook all events EXCEPT server.poll and *.event-end types
         * (the 4th param inverts the match) */
        obj.eventPump.addHook ([{type: "poll", set: /^(server|dcc-chat)$/},
                               {type: "event-end"}], event_tracer,
                               "event-tracer", true /* negate */);
    
}

/* timer-based mainloop */
function mainStep()
{

    client.eventPump.stepEvents();
    setTimeout ("mainStep()", client.STEP_TIMEOUT);
    
}

function newInlineText (data, className)
{
    var a = document.createElement ("html:a");

    if (data)
        a.appendChild (document.createTextNode (data));
    if (typeof className != "undefined")
        a.setAttribute ("class", className);

    return a;
    
}

function setCurrentObject (obj)
{
    if (!obj.messages)
    {
        dd ("** INVALID OBJECT passed to setCurrentObject **");
        return false;
    }

    var tb = getTBForObject(client.currentObject);
    if (tb)
        tb.setAttribute ("src", client.NACT_IMG);

    var output = document.getElementById ("output");

    output.removeChild (output.firstChild);
    output.appendChild (obj.messages);

    var quickList = document.getElementById ("quickList");
    if (!obj.list)
        obj.list = new CListBox();
    
    quickList.removeChild (quickList.firstChild);
    quickList.appendChild (obj.list.listContainer);
        
    client.currentObject = obj;
    tb = getTBForObject(obj);
    if (tb)
        tb.setAttribute ("src", client.CUR_IMG);
    
}

function addHistory (source, obj)
{
    if (!source.messages)
    {
        source.messages = document.createElement ("span");
        source.messages.setAttribute ("class", "chat-view");
        source.messages.setAttribute ("type", source.TYPE);
        
        switch (source.TYPE)
        {
            case "IRCChanUser":
            case "IRCUser":
                source.messages.setAttribute ("nick", source.nick);
                break;

            case "IRCNetwork":
            case "IRCChannel":
            case "IRCClient":
                source.messages.setAttribute ("name", source.name);
                break;

            default:
                dd ("** 'source' has INVALID TYPE in addHistory **");
                break;
        }       
    }

    if (client.PRINT_DIRECTION == 1)
        source.messages.appendChild (obj);
    else
        source.messages.insertBefore (obj, source.messages.firstChild);
    
    if (source.MAX_MESSAGES)
    {
        if (typeof source.messageCount != "number")
            source.messageCount = 1;
        else
            source.messageCount++;

        if (source.messageCount > source.MAX_MESSAGES)
            source.messages.removeChild (source.messages.lastChild);
    }
    
}

function notifyActivity (source)
{
    var tb = getTBForObject (source, true);
    
    if (client.currentObject != source)
        tb.setAttribute ("src", client.ACT_IMG);
    
}

/* gets the toolbutton associated with an object
 * if |create| is present, and true, create if not found */
function getTBForObject (source, create)
{
    var name;

    create = (typeof create != "undefined") ? Boolean(create) : false;

    switch (source.TYPE)
    {
        case "IRCChanUser":
        case "IRCUser":
            name = source.nick;
            break;
            
        case "IRCNetwork":
        case "IRCChannel":
        case "IRCClient":
            name = source.name;
            break;

        default:
            dd ("** INVALID OBJECT passed to getTBForObject **");
            return;
    }

    var tb, id = "tb[" + name + "]";
    var matches = 1;

    for (var i in client.viewsArray)
        if (client.viewsArray[i].source == source)
            tb = client.viewsArray[i].tb;
        else
            if (client.viewsArray[i].tb.id == id)
                id = "tb[" + name + "<" + (++matches) + ">]";

    if (!tb && create) /* not found, create one */
    {
        var views = document.getElementById ("views-tbar");
        var tbi = document.createElement ("toolbaritem");
        tbi.setAttribute ("class", "activity-button");
        tbi.setAttribute ("onclick", "onTBIClick('" + id + "')");
    
        tb = document.createElement ("titledbutton");
        tb.setAttribute ("id", id);
        client.viewsArray.push ({source: source, tb: tb});
        tb.setAttribute ("viewKey", client.viewsArray.length - 1);
        if (matches > 1)
            tb.setAttribute ("value", name + "<" + matches + ">");
        else
            tb.setAttribute ("value", name);
        tbi.appendChild (tb);
        views.appendChild (tbi);
    }

    return tb;
    
}

function filterOutput (msg, msgtype)
{

    for (var f in client.outputFilters)
        if (client.outputFilters[f].enabled)
            msg = client.outputFilters[f].func(msg, msgtype);

    client.currentObject.display (msg, msgtype, "!ME");
    
    return msg;
    
}            
    
client.sayToCurrentTarget =
function cli_say(msg)
{

    switch (client.currentObject.TYPE)
    {
        case "IRCChannel":
        case "IRCUser":
        case "IRCChanUser":
            msg = filterOutput (msg, "PRIVMSG");
            client.currentObject.say (msg);
            break;

        case "IRCClient":
            client.onInputEval ({inputData: msg});
            break;

        default:
            client.display ("No default action for objects of type '" +
                            client.currentObject.TYPE + "'", "ERROR");
            break;
    }

}

client.display =
function cli_display (message, msgtype)
{
    var ary = message.split ("\n");

    var msgContainer = newInlineText ("", "msg");
    msgContainer.setAttribute ("network", "{LOCAL}");
    msgContainer.setAttribute ("msgtype", msgtype);
    
    var msg = newInlineText ("[" + msgtype + "]", "msg-type");
    msg.setAttribute ("network", "{LOCAL}");
    msg.setAttribute ("msgtype", msgtype);
    msgContainer.appendChild(msg);

    for (var l in ary)
    {
        var msg = newInlineText (ary[l], "msg-data");
        msg.setAttribute ("network", "{LOCAL}");
        msg.setAttribute ("msgtype", msgtype);
        msgContainer.appendChild(msg);
        msgContainer.appendChild (document.createElement ("html:br"));
    }

    addHistory (this, msgContainer);        
    notifyActivity (this);

}

client.quit =
function cli_quit (reason)
{
    
    for (var n in client.networks)
        if (client.networks[n].primServ)
            client.networks[n].quit (reason);
    
}

CIRCNetwork.prototype.display =
function net_display (message, msgtype)
{
    var ary = message.split ("\n");

    var msgContainer = newInlineText ("", "msg");
    msgContainer.setAttribute ("network", this.name);
    msgContainer.setAttribute ("msgtype", msgtype);

    var msg = newInlineText ("[" + msgtype + "]", "msg-type");
    msg.setAttribute ("network", this.name);
    msg.setAttribute ("msgtype", msgtype);
    msgContainer.appendChild(msg);
        
    for (var l in ary)
    {
        var msg = newInlineText (ary[l], "msg-data");
        msg.setAttribute ("network", this.name);
        msg.setAttribute ("msgtype", msgtype);
        msgContainer.appendChild(msg);
        msgContainer.appendChild (document.createElement ("html:br"));
    }

    addHistory (this, msgContainer);
    notifyActivity (this);
            
}

CIRCUser.prototype.getDecoratedNick =
function usr_decoratednick()
{

    if (!this.decoNick)
    {
        var pfx;
        var el = newInlineText();
        
        if (this.TYPE == "IRCChanUser")
        {
            var img = document.createElement ("html:img", "option-graphic");
            img.setAttribute ("src", this.isOp ? client.OP1_IMG :
                              client.OP0_IMG);
            el.appendChild (img);
            
            img = document.createElement ("html:img", "option-graphic");
            img.setAttribute ("src", this.isVoice ? client.V1_IMG :
                              client.V0_IMG);
            el.appendChild (img);
        }
        
        el.appendChild (newInlineText (this.properNick, "option-text"));

        this.decoNick = el;
    }
    
    return this.decoNick;

}

CIRCUser.prototype.updateDecoratedNick =
function usr_updnick()
{
    var decoNick = this.getDecoratedNick();

    if (!decoNick)
        return;
    
    if (this.TYPE == "IRCChanUser")
    {
        var obj = decoNick.firstChild;
        obj.setAttribute ("src", this.isOp ? client.OP1_IMG :
                          client.OP0_IMG);

        obj = obj.nextSibling;
        obj.setAttribute ("src", this.isVoice ? client.V1_IMG :
                          client.V0_IMG);
        
        obj = obj.nextSibling;
        obj.firstChild.data = this.properNick;
    }
    
}

CIRCUser.prototype.display =
function user_display(message, msgtype)
{
    var ary = message.split ("\n");

    dd ("user.display");

    var msgContainer = newInlineText ("", "msg");
    msgContainer.setAttribute ("network", this.parent.parent.name);
    msgContainer.setAttribute ("user", this.nick);
    msgContainer.setAttribute ("msgtype", msgtype);

    var msg = newInlineText ("[" + msgtype + "]", "msg-type");
    msg.setAttribute ("network", this.parent.parent.name);
    msg.setAttribute ("user", this.nick);
    msg.setAttribute ("msgtype", msgtype);
    msgContainer.appendChild(msg);
    
    if (this.TYPE == "IRCUser")
    {
        for (var l in ary)
        {
            var msg = newInlineText (ary[l], "msg-data");
            msg.setAttribute ("network", this.parent.parent.name);
            msg.setAttribute ("user", this.nick);
            msg.setAttribute ("msgtype", msgtype);
            msgContainer.appendChild(msg);
            msgContainer.appendChild (document.createElement ("html:br"));
        }
        addHistory (this, msgContainer);
        notifyActivity (this);

    }
    else
    {
        this.parent.display (message, msgtype, this.nick);
    }
    
}

CIRCChannel.prototype.display =
function chan_display (message, msgtype, nick)
{
    var ary = message.split ("\n");

    dd ("channel.display");
    
    var msgContainer = newInlineText ("", "msg");
    msgContainer.setAttribute ("network", this.parent.parent.name);
    msgContainer.setAttribute ("channel", this.name);
    msgContainer.setAttribute ("user", nick);
    msgContainer.setAttribute ("msgtype", msgtype);

    var msg = newInlineText ("[" + msgtype + "]", "msg-type");
    msg.setAttribute ("network", this.parent.parent.name);
    msg.setAttribute ("channel", this.name);
    msg.setAttribute ("user", nick);
    msg.setAttribute ("msgtype", msgtype);
    msgContainer.appendChild (msg);
    
    for (var l in ary)
    {
        var msg = newInlineText (ary[l], "msg-data");
        msg.setAttribute ("network", this.parent.parent.name);
        msg.setAttribute ("channel", this.name);
        msg.setAttribute ("user", nick);
        msg.setAttribute ("msgtype", msgtype);
        
        if (nick)
        {
            var nickText;
            var realNick = (nick == "!ME") ? this.parent.me.properNick
                : this.users[nick].properNick;
            
            switch (msgtype)
            {
                
                case "PRIVMSG":
                    nickText = newInlineText ("<" + realNick + "> ",
                                              "msg-user");
                    break;
                    
                case "ACTION":
                    nickText = newInlineText ("*" + realNick + "* ",
                                              "msg-user");
                    break;
                    
                case "NOTICE":
                    nickText = newInlineText ("[" + realNick + "] ",
                                              "msg-user");
                    break;
                    
            }

            if (nickText)
            {
                nickText.setAttribute ("network", this.parent.parent.name);
                nickText.setAttribute ("channel", this.name);
                nickText.setAttribute ("user", nick);
                nickText.setAttribute ("msgtype", msgtype);
                msgContainer.appendChild (nickText);
            }

            if (nick == "!ME")
                addHistory (this.users[this.parent.me.nick], msgContainer);
            else
                addHistory (this.users[nick], msgContainer);
            
        }

        msgContainer.appendChild (msg);
        msgContainer.appendChild (document.createElement ("html:br"));
    }

    addHistory (this, msgContainer);
    notifyActivity (this);

}
