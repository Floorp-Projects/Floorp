/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is JSIRC Test Client #3
 *
 * The Initial Developer of the Original Code is New Dimensions Consulting,
 * Inc. Portions created by New Dimensions Consulting, Inc. are
 * Copyright (C) 1999 New Dimenstions Consulting, Inc. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 *
 * Contributor(s):
 *  Robert Ginda, rginda@ndcico.com, original author
 */

function onLoad()
{
    
    initHost(client);
    readIRCPrefs();
    setOutputStyle (client.DEFAULT_STYLE); 
    /* 
       Called in a callback once the document loads due to a current bug
    */
    mainStep();
    
}

function onUnload()
{

    client.quit ("ChatZilla! [" + navigator.userAgent + "]");
    
}

function onNotImplemented()
{

    alert ("'We're accepting patches'");
    
}

/* toolbaritem click */
function onTBIClick (id)
{
    
    var tbi = document.getElementById (id);
    var view = client.viewsArray[tbi.getAttribute("viewKey")];

    setCurrentObject (view.source);
    
}

/* popup click in user list */
function onUserListPopupClick (e)
{

    var code = e.target.getAttribute("code");
    var ary = code.substr(1, code.length).match (/(\S+)? ?(.*)/);    

    if (!ary)
        return;

    var command = ary[1];

    var ev = new CEvent ("client", "input-command", client,
                         "onInputCommand");
    ev.command = command;
    ev.inputData =  ary[2] ? stringTrim(ary[2]) : "";    
    ev.target = client.currentObject;

    getObjectDetails (ev.target, ev);

    client.eventPump.addEvent (ev);
}


function onToggleTraceHook()
{
    var h = client.eventPump.getHook ("event-tracer");
    
    h.enabled = !h.enabled;
    document.getElementById("menu-dmessages").setAttribute ("checked",
                                                            h.enabled);
    
}   

function onToggleToolbarIcons()
{
    client.ICONS_IN_TOOLBAR = !client.ICONS_IN_TOOLBAR;
    
    var newclass = "menubutton " + ((client.ICONS_IN_TOOLBAR) ?
        "activity-button-image" : "activity-button-text");

    for (var i in client.viewsArray)
        client.viewsArray[i].tb.setAttribute ("class", newclass);

}

function onToggleVisibility(thing)
{    
    var menu = document.getElementById("menu-view-" + thing);
    var ids = new Array();
    
    switch (thing)
    {
        case "toolbar":
            ids = ["views-tbox"];
            break;
            
        case "info":
            ids = ["user-list-box", "main-splitter"];            
            break;
            
        case "status":
            ids = ["status-bar-tbox"];
            break;

        default:
            dd ("** Unknown element '" + menuId + 
                "' passed to onToggleVisibility. **");
            return;
    }


    var newState;
    var elem = document.getElementById(ids[0]);
    var d = elem.getAttribute("collapsed");
    
    if (d == "true")
    {
        newState = "false";
        menu.setAttribute ("checked", "true");
        client.uiState[thing] = true;
    }
    else
    {
        newState = "true";
        menu.setAttribute ("checked", "false");
        client.uiState[thing] = false;
    }
    
    for (var i in ids)
    {
        elem = document.getElementById(ids[i]);
        elem.setAttribute ("collapsed", newState);
    }

    updateTitle();

}

function onDoStyleChange (newStyle)
{

    if (newStyle == "other")
        newStyle = window.prompt ("Enter stylesheet filename " +
                                  "(relative to chrome://chatzilla/skin/)");

    if (newStyle)
    {
        setOutputStyle (newStyle);
        setCurrentObject(client.currentObject);
    }
    
}

function onHideCurrentView()
{
    var tb = getTBForObject(client.currentObject);
    
    if (tb)
    {
        var i = deleteToolbutton (tb);
        if (i != -1)
        {
            if (i >= client.viewsArray.length)
                i = client.viewsArray.length - 1;
            
            setCurrentObject (client.viewsArray[i].source);
        }
        
    }
    
}

function onDeleteCurrentView()
{
    var tb = getTBForObject(client.currentObject);
    
    if (tb)
    {
        var i = deleteToolbutton (tb);
        if (i != -1)
        {
            delete client.currentObject.messages;

            if (i >= client.viewsArray.length)
                i = client.viewsArray.length - 1;            
            setCurrentObject (client.viewsArray[i].source);
        }
        
    }
    
}

function onClearCurrentView()
{

    if (client.output.firstChild)
        client.output.removeChild (client.output.firstChild);

    client.currentObject.messages = (void 0);
    delete client.currentObject.messages;

    client.currentObject.display ("Messages Cleared.", "INFO");

    client.output.appendChild (client.currentObject.messages);
    
}

function onSortCol(sortColName)
{
    const nsIXULSortService = Components.interfaces.nsIXULSortService;
    const isupports_uri = "@mozilla.org/rdf/xul-sort-service;1";
    
    var node = document.getElementById(sortColName);
    // determine column resource to sort on
    var sortResource = node.getAttribute('resource');
    if (!node)
        return false;
 
    var sortDirection = "ascending";
        //node.setAttribute("sortActive", "true");

    switch (sortColName)
    {
        case "usercol-op":
            document.getElementById("usercol-voice")
                .setAttribute("sortDirection", "natural");
            document.getElementById("usercol-nick")
                .setAttribute("sortDirection", "natural");
            break;
        case "usercol-voice":
            document.getElementById("usercol-op")
                .setAttribute("sortDirection", "natural");
            document.getElementById("usercol-nick")
                .setAttribute("sortDirection", "natural");
            break;
        case "usercol-nick":
            document.getElementById("usercol-voice")
                .setAttribute("sortDirection", "natural");
            document.getElementById("usercol-op")
                .setAttribute("sortDirection", "natural");
            break;
    }
    
    var currentDirection = node.getAttribute('sortDirection');
    
    if (currentDirection == "ascending")
        sortDirection = "descending";
    else if (currentDirection == "descending")
        sortDirection = "natural";
    else
        sortDirection = "ascending";
    
    node.setAttribute ("sortDirection", sortDirection);
 
    var xulSortService =
        Components.classes[isupports_uri].getService(nsIXULSortService);
    if (!xulSortService)
        return false;
    try
    {
        xulSortService.Sort(node, sortResource, sortDirection);
    }
    catch(ex)
    {
            //dd("Exception calling xulSortService.Sort()");
    }
    
    return false;
}

function onToggleMunger()
{

    client.munger.enabled = !client.munger.enabled;
    document.getElementById("menu-munger").setAttribute ("checked",
                                                         client.munger.enabled);

}

function onInputKeyUp (e)
{
    
    switch (e.keyCode)
    {        
        case 13: /* CR */
            e.line = e.target.value;
            onInputCompleteLine (e);
            break;

        case 38: /* up */
            if (client.lastHistoryReferenced < client.inputHistory.length - 1)
                e.target.value =
                    client.inputHistory[++client.lastHistoryReferenced];
            break;

        case 40: /* down */
            if (client.lastHistoryReferenced > 0)
                e.target.value =
                    client.inputHistory[--client.lastHistoryReferenced];
            else
            {
                client.lastHistoryReferenced = -1;
                e.target.value = client.incompleteLine;
            }
            
            break;

        case 9: /* tab */
            e.preventDefault();
            onTabCompleteRequest(e);
            break;       
            
        default:
            client.incompleteLine = e.target.value;

            
    }

}

function onTabCompleteRequest (e)
{
    var selStart = e.target.selectionStart;
    var selEnd = e.target.selectionEnd;            
    var v = e.target.value;

    if (selStart != selEnd) 
    {
        /* text is highlighted, just move caret to end and exit */
        e.target.selectionStart = e.target.selectionEnd = v.length;
        return;
    }

    var firstSpace = v.indexOf(" ");
    if (firstSpace == -1)
        firstSpace = v.length;
    
    if ((v[0] == client.COMMAND_CHAR) && (selStart <= firstSpace))
    {
        /* complete a command */                
        var partialCommand = v.substring(1, firstSpace).toLowerCase();
        var cmds = client.commands.listNames(partialCommand);
            
        if (cmds.length == 1)
        {
            /* partial matched exactly one command */
            var pfx = client.COMMAND_CHAR + cmds[0];
            if (firstSpace == v.length)
                v =  pfx + " ";
            else
                v = pfx + v.substr (firstSpace);
            
            e.target.value = v;
            e.target.selectionStart = e.target.selectionEnd = 
                pfx.length + 1;
        }
        else if (cmds.length > 1)
        {
            /* partial matched more than one command */
            var d = new Date();
            if ((d - client.lastTabUp) <= client.DOUBLETAB_TIME)
                client.currentObject.display
                    ("Commands matching ``" + partialCommand + 
                     "'' are [" + cmds.join(", ") + "]", "INFO");
            else
                client.lastTabUp = d;
            
            var pfx = client.COMMAND_CHAR + getCommonPfx(cmds);
            if (firstSpace == v.length)
                v =  pfx;
            else
                v = pfx + v.substr (firstSpace);
            
            e.target.value = v;
            e.target.selectionStart = e.target.selectionEnd = pfx.length;
            
        }
        else
            client.currentObject.display ("No commands matching " +
                                          partialCommand, "ERROR");
        
    }
    else if (client.currentObject.users)
    {
        /* complete a nickname */
        var users = client.currentObject.users;
        var nicks = new Array();
        
        for (var n in users)
            nicks.push (users[n].nick);
        
        var nickStart = v.lastIndexOf(" ", selStart) + 1;
        var nickEnd = v.indexOf (" ", selStart);
        if (nickEnd == -1)
            nickEnd = v.length;
        
        var partialNick = v.substring(nickStart, nickEnd).toLowerCase();
        
        var matchingNicks = matchEntry (partialNick, nicks);
        
        if (matchingNicks.length > 0)
        {
            var subst;
            
            if (matchingNicks.length == 1)
            {   /* partial matched exactly one nick */
                subst = 
                    client.currentObject.users[matchingNicks[0]].properNick;
                if (nickStart == 0)
                    subst += client.ADDRESSED_NICK_SEP;
                else
                    subst += " ";
            }
            else
            {   /* partial matched more than one command */
                var d = new Date();
                if ((d - client.lastTabUp) <= client.DOUBLETAB_TIME)
                    client.currentObject.display
                        ("Users matching ``" + partialNick +
                         "'' are [" + matchingNicks.join(", ") + "]",
                         "INFO");
                else
                    client.lastTabUp = d;
                
                subst = getCommonPfx(matchingNicks);
            }
            
            v = v.substr (0, nickStart) + subst + v.substr (nickEnd);
            e.target.value = v;
            
        }
        
    }

}

function onWindowKeyPress (e)
{
    var code = Number (e.keyCode)
    switch (code)
    {
        case 112: /* F1 */
        case 113: /* ... */
        case 114:
        case 115:
        case 116:
        case 117:
        case 118:
        case 119:
        case 120:
        case 121: /* F10 */
            var idx = code - 112;
            if ((client.viewsArray[idx]) && (client.viewsArray[idx].source))
                setCurrentObject(client.viewsArray[idx].source);
            
            return false;
            break;

        case 33: /* pgup */
            var w = window.frames[0];
            var newOfs = w.pageYOffset - (w.innerHeight / 2);
            if (newOfs > 0)
                w.scrollTo (w.pageXOffset, newOfs);
            else
                w.scrollTo (w.pageXOffset, 0);
            break;
            
        case 34: /* pgdn */
            var w = window.frames[0];
            var newOfs = w.pageYOffset + (w.innerHeight / 2);
            if (newOfs < (w.innerHeight + w.pageYOffset))
                w.scrollTo (w.pageXOffset, newOfs);
            else
                w.scrollTo (w.pageXOffset, (w.innerHeight + w.pageYOffset));
            break;

        case 9: /* tab */
            e.preventDefault();
            break;
            
        default:
            
    }

}

function onInputCompleteLine(e)
{

    if (e.target.getAttribute ("expanded") != "YES")
    {
        e.line = e.line.replace (/\n/g, "");

        if (client.inputHistory[0] != e.line)
            client.inputHistory.unshift (e.line);

        if (client.inputHistory.length > client.MAX_HISTORY)
            client.inputHistory.pop();
        
        client.lastHistoryReferenced = -1;
        client.incompleteLine = "";
        
        if (e.line[0] == client.COMMAND_CHAR)
        {
            var ary = e.line.substr(1, e.line.length).match (/(\S+)? ?(.*)/);
            var command = ary[1];
            
            if (command[0].search (/[\[\{\(]/) == 0) /* request to expand */
            {
                e.target.setAttribute("expanded", "YES");
                switch (command[0])
                {
                    case "[":
                        e.target.setAttribute("collapseChar", "]");
                        break;

                    case "{":
                        e.target.setAttribute("collapseChar", "}");
                        break;
                        
                    case "(":
                        e.target.setAttribute("collapseChar", ")");
                        break;        
                }
                e.target.style.height = client.EXPAND_HEIGHT;
            }
            else /* normal command */
            {
                var ev = new CEvent ("client", "input-command", client,
                                     "onInputCommand");
                ev.command = command;
                ev.inputData =  ary[2] ? stringTrim(ary[2]) : "";

                ev.target = client.currentObject;
                getObjectDetails (ev.target, ev);
                client.eventPump.addEvent (ev);
                e.target.value = "";            
            }
        }
        else /* plain text */
        {
            client.sayToCurrentTarget (e.line);
            e.target.value = "";            
        }
    }
    else /* input-box is expanded */
    {
        var lines = e.target.value.split("\n");
        for (i in lines)
            if (lines[i] == "")
                arrayRemoveAt (lines, i);
        var lastLine = lines[lines.length - 1];

        if (lastLine.replace(/s*$/,"") ==
            e.target.getAttribute ("collapseChar"))
        {
            dd ("collapsing...");
            
            e.target.setAttribute("expanded", "NO");
            e.target.style.height = client.COLLAPSE_HEIGHT;
            e.target.value = "";
            client.sayToCurrentTarget (lines[i]);
            e.target.value = "";            
        }
    }
    
}


client.onInputCommand = 
function cli_icommand (e)
{
    var ary = client.commands.list (e.command);
    
    switch (ary.length)
    {        
        case 0:
            client.currentObject.display ("Unknown command '" + e.command +
                                          "'.", "ERROR");
            break;
            
        case 1:
            if (typeof client[ary[0].func] == "undefined")        
                client.currentObject.display ("Sorry, '" + ary[0].name +
                                              "' has not been implemented.", 
                                              "ERROR");
            else
            {
                e.commandEntry = ary[0];
                if (!client[ary[0].func](e))
                    client.currentObject.display (ary[0].name + " " +
                                                  ary[0].usage, "USAGE");
            }
            break;
            
        default:
            client.currentObject.display ("Ambiguous command: '" + e.command +
                                          "'", "ERROR");
            var str = "";
            for (var i in ary)
                str += str ? ", " + ary[i].name : ary[i].name;
            client.currentObject.display (ary.length + " commands match: " +
                                          str, "ERROR");
    }

}

client.onInputHelp =
function cli_ihelp (e)
{
    var ary = client.commands.list (e.inputData);
 
    if (ary.length == 0)
    {
        client.currentObject.display ("No such command, '" + e.inputData +
                                      "'.", "ERROR");
        return false;
    }

    var saveDir = client.PRINT_DIRECTION;
    client.PRINT_DIRECTION = 1;
    
    for (var i in ary)
    {        
        client.currentObject.display (ary[i].name + " " + ary[i].usage,
                                      "USAGE");
        client.currentObject.display (ary[i].help, "HELP");
    }

    client.PRINT_DIRECTION = saveDir;
    
    return true;
    
}

client.onInputTestDisplay =
function cli_testdisplay (e)
{

    client.currentObject.display ("Hello World!", "HELLO");
    client.currentObject.display ("Nothing is wrong.", "ERROR");
    client.currentObject.display ("Use not, want not.", "USAGE");
    client.currentObject.display ("Don't Panic", "HELP");
    client.currentObject.display ("NOTICE this!", "NOTICE", "Mozilla");
    client.currentObject.display ("And hear this.", "PRIVMSG", "Mozilla");
    client.currentObject.display ("But dont do this?", "ACTION", "Mozilla");
    client.currentObject.display ("or you'll get KICKed", "KICK", "Mozilla");
    client.currentObject.display ("JOIN in the fun.", "JOIN", "Mozilla");
    client.currentObject.display ("PART when you want.", "PART", "Mozilla");
    client.currentObject.display ("But never QUIT", "QUIT", "Mozilla");

    if (client.currentObject.TYPE == "IRCChannel")
    {
        var mynick = e.server.me.nick;
        client.currentObject.display ("NOTICE this!", "NOTICE", "!ME");
        client.currentObject.display ("And hear this.", "PRIVMSG", "!ME");
        client.currentObject.display ("But dont do this?", "ACTION", "!ME");
        client.currentObject.display ("or you'll get KICKed", "KICK", "!ME");
        client.currentObject.display ("JOIN in the fun.", "JOIN", "!ME");
        client.currentObject.display ("PART when you want.", "PART", "!ME");
        client.currentObject.display ("But never QUIT", "QUIT", "!ME");
    }

    return true;
    
}   

client.onInputNetwork =
function cli_inetwork (e)
{
    if (!e.inputData)
        return false;

    var net = client.networks[e.inputData];

    if (net)
    {
        client.lastNetwork = net;
        setCurrentObject (net);    
    }
    else
    {
        client.currentObject.display ("Unknown network '" + e.inputData + "'",
                                      "ERROR");
        return false;
    }
    
    return true;
    
}

client.onInputServer =
function cli_iserver (e)
{
    if (!e.inputData)
        return false;

    var ary = e.inputData.match(/^([^\s\:]+)[\s\:]?(\d+)? ?(\S+)?/);
    var pass = (ary[2]) ? ary[2] : "";
    
    if (ary == null)
        return false;

    if (!ary[2])
        ary[2] = 6667;
    
    client.networks[ary[1]] =
        new CIRCNetwork (ary[1], [{name: ary[1], port: ary[2]}],
                         client.eventPump);
    
    client.onInputAttach ({inputData: ary[1] + " " + pass});
    
    return true;

}

client.onInputQuit =
function cli_quit (e)
{
    if (!e.server)
    {
        client.currentObject.display ("Quit can only be used in the context " +
                                      "of a network, perhaps you meant /exit?",
                                      "ERROR");
        return false;
    }

    if (!e.server.connection.isConnected)
    {
        client.currentObject.display ("Not connected", "ERROR");
        return false;
    }

    e.server.logout (e.inputData);

    return true;
    
}

client.onInputExit =
function cli_exit (e)
{
    
    client.quit(e.inputData);    
    window.close();
    
}

client.onInputDelete =
function cli_idelete (e)
{
    
    onDeleteCurrentView();
    return true;

}

client.onInputHide=
function cli_ihide (e)
{
    
    onHideCurrentView();
    return true;

}

client.onInputClear =
function cli_iclear (e)
{
    
    onClearCurrentView();
    return true;

}

client.onInputNames =
function cli_inames ()
{
    
    onToggleVisibility ("info");
    return true;
    
}

client.onInputToolbar =
function cli_itbar ()
{
    
    onToggleVisibility ("toolbar");
    return true;

}

client.onInputStatusbar =
function cli_isbar ()
{
    
    onToggleVisibility ("status");
    return true;

}

client.onInputCommands =
function cli_icommands (e)
{
    
    if (e && e.inputData)
        client.currentObject.display ("Currently implemented commands " +
                                      "matching the pattern ``" + e.inputData +
                                      "'' are ["  + 
                                      client.commands.listNames(e.inputData)
                                      .join(", ") + "].", "INFO");
    else
        client.currentObject.display ("Currently implemented commands are ["  + 
                                      client.commands.listNames().join(", ") +
                                      "].", "INFO");
    return true;
}

client.onInputAttach =
function cli_iattach (e)
{
    var net;
    var pass;
    
    if (!e.inputData)
    {
        if (client.lastNetwork)
        {        
            client.currentObject.display ("No network specified network, " +
                                          "Using '" + client.lastNetwork.name +
                                          "'", "NOTICE");
            net = client.lastNetwork;
        }
        else
        {
            client.currentObject.display ("No network specified, and no " +
                                          "default network is in place.",
                                          "ERROR");
            return false;
        }
    }
    else
    {
        var ary = e.inputData.match (/(\S+) ?(\S+)?/);
        net = client.networks[ary[1]];
        pass = ary[2];
        
        if (!net)
        {
            client.currentObject.display ("Unknown network '" +
                                          e.inputData + "'", "ERROR");
            return false;
        }
        client.lastNetwork = net;
    }

    if (CIRCNetwork.prototype.INITIAL_NICK == client.defaultNick)
        CIRCNetwork.prototype.INITIAL_NICK =
            prompt ("Please select a nickname", client.defaultNick);
    
    net.connect(pass);
    net.display ("Connecting...", "INFO");
    setCurrentObject(net);
    return true;
    
}
    
client.onInputMe =
function cli_ime (e)
{
    if (!e.channel)
    {
        client.currentObject.display ("Me can only be used from channels.",
                                      "ERROR");
        return false;
    }

    e.inputData = filterOutput (e.inputData, "ACTION", "!ME");
    e.channel.act (e.inputData);
    
    return true;
}

client.onInputMsg =
function cli_imsg (e)
{

    if (!e.network || !e.network.isConnected())
    {
        client.currentObject.display ("You must be connected to a network " +
                                      "to use msg", "ERROR");
        return false;
    }

    var ary = e.inputData.match (/(\S+)\s+(.*)/);
    if (ary == null)
        return false;

    var usr = e.network.primServ.addUser(ary[1].toLowerCase());

    if (!usr.messages)
        usr.display ("Chat with " + usr.nick + " opened.", "INFO");
    setCurrentObject (usr);
    var msg = filterOutput(ary[2], "PRIVMSG", "!ME");
    usr.say (ary[2]);

    return true;

}

client.onInputNick =
function cli_inick (e)
{

    if (!e.inputData)
        return false;
    
    if (e.server) 
        e.server.sendData ('NICK ' + e.inputData + '\n');
    else
        CIRCNetwork.prototype.INITIAL_NICK = e.inputData;
    
    return true;
    
    
}

client.onInputName =
function cli_iname (e)
{

    if (!e.inputData)
        return false;
    
    CIRCNetwork.prototype.INITIAL_NAME = e.inputData;

    return true;
    
}

client.onInputDesc =
function cli_idesc (e)
{

    if (!e.inputData)
        return false;
    
    CIRCNetwork.prototype.INITIAL_DESC = e.inputData;
    
    return true;
    
}

client.onInputQuote =
function cli_iquote (e)
{
    if (!e.network || !e.network.isConnected())
    {
        client.currentObject.display ("You must be connected to a network " +
                                      "to use quote.", "ERROR");
        return false;
    }

    e.server.sendData (e.inputData + "\n");
    
    return true;
    
}

client.onInputEval =
function cli_ieval (e)
{
    if (!e.inputData)
        return false;
    
    if (e.inputData.indexOf ("\n") != -1)
        e.inputData = "\n" + e.inputData + "\n";
    
    try
    {
        client.currentObject.doEval = function (s) { return eval(s); }
        
        rv = String(client.currentObject.doEval (e.inputData));
        if (rv.indexOf ("\n") == -1)
            client.currentObject.display ("{" + e.inputData + "} " + rv,
                                          "EVAL");
        else
            client.currentObject.display ("{" + e.inputData + "}\n" + rv,
                                          "EVAL");
    }
    catch (ex)
    {
        client.currentObject.display (String(ex), "ERROR");
    }
    
    return true;
    
}

client.onInputCTCP =
function cli_ictcp (e)
{
    if (!e.inputData)
        return false;

    if (!e.server)
    {
        client.currentObject.display ("You must be connected to a server to " +
                                      "use CTCP.", "ERROR");
        return false;
    }

    var ary = e.inputData.match(/^(\S+) (\S+)$/);
    if (ary == null)
        return false;
    
    e.server.ctcpTo (ary[1], ary[2]);
    
    return true;
    
}


client.onInputJoin =
function cli_ijoin (e)
{
    if (!e.network || !e.network.isConnected())
    {
        if (!e.network)
            client.currentObject.display ("No network selected.", "ERROR");
        else
            client.currentObject.display ("Network '" + e.network.name +
                                          " is not connected.", "ERROR");        
        return false;
    }
    
    var ary = e.inputData.match(/(\S+) ?(\S+)?/);
    if (!ary)
        return false;
    
    var name = ary[1];
    var key = (ary[2]) ? ary[2] : "";

    if ((name[0] != "#") && (name[0] != "&") && (name[0] != "+"))
        name = "#" + name;

    e.channel = e.server.addChannel (name);
    e.channel.join(key);
    e.channel.display ("Joining...", "INFO");
    setCurrentObject(e.channel);
    
    return true;
    
}

client.onInputLeave =
function cli_ipart (e)
{
    if (!e.channel)
    {            
        client.currentObject.display ("Leave can only be used from channels.",
                                      "ERROR");
        return false;
    }

    e.channel.part();

    return true;
    
}

client.onInputZoom =
function cli_izoom (e)
{
    client.currentObject.display ("**WARNING** Zoom is busted at this time :(",
                                  "WARNING");

    if (!e.inputData)
        return false;
    
    if (!e.channel)
    {
        client.currentObject.display ("Zoom can only be used from channels.",
                                     "ERROR");
        return false;
    }
    
    var cuser = e.channel.getUser(e.inputData);
    
    if (!cuser)
    {
        client.currentObject.display ("User '" + e.inputData + "' not found.",
                                      "ERROR");
        return false;
    }
    
    setCurrentObject(cuser);

    return true;
    
}    

/**
 * Performs a whois on a user.
 */
client.onInputWhoIs = 
function cli_whois (e) 
{
    if (!e.network || !e.network.isConnected())
    {
        client.currentObject.display ("You must be connected to a network " +
                                      "to use whois", "ERROR");
        return false;
    }

    if (!e.inputData)
    {
        var nicksAry = e.channel.getSelectedUsers();
 
        if (nicksAry)
        {
            dd ("Sending [" + nicksAry.length + "] nicks the whois call\n");
            mapObjFunc(nicksAry, "whois", null);
            return true;
        }
        else
        {
            return false;
        }
    }
    // Otherwise, there is no guarantee that the username
    // is currently a user
    var nick = e.inputData.match( /\S+/ );

    e.server.whois (nick);
    
    return true;
}

client.onInputTopic =
function cli_itopic (e)
{
    if (!e.channel)
    {
        client.currentObject.display ("Topic can only be used from channels.",
                                      "ERROR");
        return false;
    }
    
    if (!e.inputData)
    {
        if (e.channel.topic)
        {
            client.currentObject.display ("Topic: " + e.channel.topic,
                                          "TOPIC");
            client.currentObject.display ("Set by " + e.channel.topicBy +
                                          " on " + e.channel.topicDate + ".",
                                          "TOPIC");
        }
        else
            client.currentObject.display ("No topic.", "TOPIC");
    }
    else
    {
        if (!e.channel.setTopic(e.inputData))
            client.currentObject.display ("Could not set topic.", "ERROR");
    }

    return true;
    
}

client.onInputAway =
function cli_iaway (e)
{ 
    if (!e.network || !e.network.isConnected())
    {
        client.currentObject.display ("You must be connected to a network " +
                                      "to use away.", "ERROR");
        return false;
    }
    else if (!e.inputData) 
    {
        e.server.sendData ("AWAY\n");
    }
    else
    {
        e.server.sendData ("AWAY " + e.inputData + "\n");
    }

    return true;
}    

/**
 * Removes operator status from a user.
 */
client.onInputDeop = 
function cli_ideop (e) 
{
    /* NOTE: See the next function for a well commented explanation
       of the general form of these Multiple-Target type functions */

    if (!e.channel)
    {
        client.currentObject.display ("You must be on a channel to use " +
                                      "to use deop.", "ERROR");
        return false;
    }    
    
    if (!e.inputData)
    {
        var nicksAry = e.channel.getSelectedUsers();

 
        if (nicksAry)
        {
            mapObjFunc(nicksAry, "setOp", false);
            return true;
        }
        else
        {
            return false;
        }
    }

    var cuser = e.channel.getUser(e.inputData);
    
    if (!cuser)
    {
        /* use e.inputData so the case is retained */
        client.currentObject.display ("User '" + e.inputData + "' not found.",
                                      "ERROR");
        return false;
    }
    
    cuser.setOp(false);

    return true;
}


/**
 * Gives operator status to a channel user.
 */
client.onInputOp = 
function cli_iop (e) 
{
    if (!e.channel)
    {
        client.currentObject.display ("You must be connected to a network " +
                                      "to use op.", "ERROR");
        return false;
    }
    
    
    if (!e.inputData)
    {
        /* Since no param is passed, check for selection */
        var nicksAry = e.channel.getSelectedUsers();

        /* If a valid array of user objects, then call the mapObjFunc */
        if (nicksAry)
        {
            /* See test3-utils.js: this simply
               applies the setOp function to every item
               in nicksAry with the parameter of "true" 
               each time 
            */
            mapObjFunc(nicksAry, "setOp", true);
            return true;
        }
        else
        {
            /* If no input and no selection, return false
               to display the usage */
            return false;
        }
    }

    /* We do have inputData, so use that, rather than any
       other option */

    var cuser = e.channel.getUser(e.inputData);
    
    if (!cuser)
    {
        client.currentObject.display ("User '" + e.inputData + "' not found.",
                                      "ERROR");
        return false;
    }
    
    cuser.setOp(true);

    return true;   
    
}

/**
 * Gives voice status to a user.
 */
client.onInputVoice = 
function cli_ivoice (e) 
{
    if (!e.channel)
    {
        client.currentObject.display ("You must be on a channel " +
                                      "to use voice.", "ERROR");
        return false;
    }    
    
    if (!e.inputData)
    {
        var nicksAry = e.channel.getSelectedUsers();

        if (nicksAry)
        {
            mapObjFunc(nicksAry, "setVoice", true);
            return true;
        }
        else
        {
            return false;
        }
    }

    var cuser = e.channel.getUser(e.inputData);
    
    if (!cuser)
    {
        client.currentObject.display ("User '" + e.inputData + "' not found.",
                                      "ERROR");
        return false;
    }
    
    cuser.setVoice(true);

    return true;
}

/**
 * Removes voice status from a user.
 */
client.onInputDevoice = 
function cli_devoice (e) 
{
    if (!e.channel)
    {
        client.currentObject.display ("You must be on a channel " +
                                      "to use devoice.", "ERROR");
        return false;
    }    
    
    if (!e.inputData)
    {
        var nicksAry = e.channel.getSelectedUsers();

        if (nicksAry)
        {
            mapObjFunc(nicksAry, "setVoice", false);
            return true;
        }
        else
        {
            return false;
        }
    }
    
    var cuser = e.channel.getUser(e.inputData);
    
    if (!cuser)
    {
        client.currentObject.display ("User '" + e.inputData + "' not found.",
                                      "ERROR");
        return false;
    }
    
    cuser.setVoice(false);

    return true;
}

/**
 * Displays input to the current view, but doesn't send it to the server.
 */
client.onInputEcho =
function cli_iecho (e)
{
    if (!e.inputData)
    {
        return false;
    }
    else 
    {
        client.currentObject.display (e.inputData, "ECHO");
        
        return true;
    }
}

client.onInputInvite =
function cli_iinvite (e) 
{

    if (!e.network || !e.network.isConnected())
    {
        client.currentObject.display ("You must be connected to a network " +
                                      "to use invite.", "ERROR");
        return false;
    }     
    else if (!e.channel)
    {
        client.currentObject.display 
        ("You must be in a channel to use invite", "ERROR");
        return false;
    }    
    
    if (!e.inputData) {
        return false;
    }
    else 
    {
        var ary = e.inputData.split( /\s+/ );
        
        if (ary.length == 1)
        {
            e.channel.invite (ary[0]);
        }
        else
        {
            var chan = e.server.channels[ary[1].toLowerCase()];

            if (chan == undefined) 
            {
                client.currentObject.display ("You must be on " + ary[1] + 
                                              " to use invite.", "ERROR");
                return false;
            }            

            chan.invite (ary[0]);
        }   
        
        return true;
    }
}


client.onInputKick =
function cli_ikick (e) 
{
    if (!e.channel)
    {
        client.currentObject.display ("You must be on a channel to use " +
                                      "kick.", "ERROR");
        return false;
    }    
    
    if (!e.inputData)
    {
        var nicksAry = e.channel.getSelectedUsers();

        if (nicksAry)
        {
            mapObjFunc(nicksAry, "kick", "");
            return true;
        }
        else
        {
            return false;
        }
    }

    var ary = e.inputData.match ( /(\S+)? ?(.*)/ );

    var cuser = e.channel.getUser(ary[1]);
    
    if (!cuser)
    {    
        client.currentObject.display ("User '" + e.inputData + "' not found.",
                                      "ERROR");
        return false;
    }

    if (ary.length > 2)
    {               
        cuser.kick(ary[2]);
    }
    else     

    cuser.kick();    
            
    return true;
}

client.onInputStalk =
function cli_istalk ( e )
{
    if (!e.inputData)
    {
        if ( client.stalkingVictims.length == 0 ) {
            client.currentObject.display( "No stalking victims.", "STALK" );
        } else {
            client.currentObject.display( "Currently stalking [" +
                                      client.stalkingVictims.join(", ") + "]",
                                      "STALK");
        }
        return true;
    }
    client.stalkingVictims[client.stalkingVictims.length] = e.inputData;
    client.currentObject.display( "Now stalking " + e.inputData, "STALK"
);
    return true;
}

client.onInputUnstalk =
function cli_iunstalk ( e )
{
    if ( !e.inputData )
        return false;

    for ( i in client.stalkingVictims ) {
        if ( client.stalkingVictims[i].match( "^" + e.inputData +"$", "i" ) ) {
            client.stalkingVictims.splice(i,1);
            client.currentObject.display( "No longer stalking " +
                e.inputData, "UNSTALK" );
            return true;
        }
    }

    client.currentObject.display( "Not stalking " + e.inputData,
        "UNSTALK" );
    return true;
}

/* 'private' function, should only be used from inside */
CIRCChannel.prototype._addUserToGraph =
function my_addtograph (user)
{
    if (!user.TYPE)
        dd (getStackTrace());
    
    client.rdf.Assert (this.getGraphResource(), client.rdf.resChanUser,
                       user.getGraphResource(), true);
    
}

/* 'private' function, should only be used from inside */
CIRCChannel.prototype._removeUserFromGraph =
function my_remfgraph (user)
{

    client.rdf.Unassert (this.getGraphResource(), client.rdf.resChanUser,
                         user.getGraphResource());
    
}

CIRCNetwork.prototype.onNotice = 
CIRCNetwork.prototype.on001 = /* Welcome! */
CIRCNetwork.prototype.on002 = /* your host is */
CIRCNetwork.prototype.on003 = /* server born-on date */
CIRCNetwork.prototype.on004 = /* server id */
CIRCNetwork.prototype.on251 = /* users */
CIRCNetwork.prototype.on252 = /* opers online (in params[2]) */
CIRCNetwork.prototype.on254 = /* channels found (in params[2]) */
CIRCNetwork.prototype.on255 = /* link info */
CIRCNetwork.prototype.on265 = /* local user details */
CIRCNetwork.prototype.on266 = /* global user details */
CIRCNetwork.prototype.on375 = /* start of MOTD */
CIRCNetwork.prototype.on372 = /* MOTD line */
CIRCNetwork.prototype.on376 = /* end of MOTD */
function my_showtonet (e)
{
    var p = (e.params[2]) ? e.params[2] + " " : "";
    var str = "";

    switch (e.code)
    {
        case "004":
            str = e.params.slice(1).join (" ");
            break;

        case "001":
            updateTitle(this);
            updateNetwork (this);
            if (this.pendingURLs)
            {
                var url = this.pendingURLs.pop();
                while (url)
                {
                    gotoIRCURL(url);
                    url = this.pendingURLs.pop();
                }
                delete this.pendingURLs;
            }
            str = e.meat;
            break;
            
        case "372":
        case "375":
        case "376":
            if (this.IGNORE_MOTD)
                return;
            /* no break */

        default:
            str = e.meat;
            break;
    }

    this.display (p + str, e.code.toUpperCase());
    
}

CIRCNetwork.prototype.on311 = /* whois name */
CIRCNetwork.prototype.on319 = /* whois channels */
CIRCNetwork.prototype.on312 = /* whois server */
CIRCNetwork.prototype.on317 = /* whois idle time */
CIRCNetwork.prototype.on318 = /* whois end of whois*/
function my_whoisreply (e)
{
    var text = "egads!";
    
    switch (Number(e.code))
    {
        case 311:
            text = e.params[2] + " (" + e.params[3] + "@" + e.params[4] +
                ") is " + e.meat;
            break;
            
        case 319:
            text = e.params[2] + " is a member of " + e.meat;
            break;
            
        case 312:
            text = e.params[2] + " is attached to " + e.params[3]
            break;
            
        case 317:
            text = e.params[2] + " has been idle for " + e.params[3] +
                " seconds (on since " + new Date(Number(e.params[4]) * 1000) +
                ")";
            break;
            
        case 318:
            text = "End of whois information for " + e.params[2];
            break;
            
    }

    e.server.parent.display(text, e.code);
    
}

CIRCNetwork.prototype.on433 = /* nickname in use */
function my_433 (e)
{

    e.server.parent.display ("The nickname '" + e.params[2] +
                             "' is already in use.", e.code);
    
}

CIRCNetwork.prototype.onError =
function my_neterror (e)
{

    e.server.parent.display (e.meat, "ERROR");
    
}

CIRCNetwork.prototype.onPing =
function my_netping (e)
{

    updateNetwork (e.network);
    
}

CIRCNetwork.prototype.onPong =
function my_netpong (e)
{

    updateNetwork (e.network);
    
}

CIRCChannel.prototype.onPrivmsg =
function my_cprivmsg (e)
{
    
    e.user.display (e.meat, "PRIVMSG");
    
    if (e.meat.indexOf (client.prefix) == 0)
    {
        try
        {
            var v = eval(e.meat.substring (client.prefix.length,
                                           e.meat.length));
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

    return true;
    
}

/* end of names */
CIRCChannel.prototype.on366 =
function my_366 (e)
{

    if (client.currentObject == this)    
        /* hide the tree while we add (possibly tons) of nodes */
        client.rdf.setTreeRoot("user-list", client.rdf.resNullChan);
    
    client.rdf.clearTargets(this.getGraphResource(), client.rdf.resChanUser);

    for (var u in this.users)
    {
        this.users[u].updateGraphResource();
        this._addUserToGraph (this.users[u]);
    }

    
    if (client.currentObject == this)
        /* redisplay the tree */
        client.rdf.setTreeRoot("user-list", this.getGraphResource());
    
}    

CIRCChannel.prototype.onTopic = /* user changed topic */
CIRCChannel.prototype.on332 = /* TOPIC reply */
function my_topic (e)
{

    if (e.code == "TOPIC")
        e.channel.display (e.channel.topicBy + " has changed the topic to '" +
                           e.channel.topic + "'", "TOPIC");
    
    updateChannel (e.channel);
    updateTitle (e.channel);
    
}


CIRCChannel.prototype.onNotice =
function my_notice (e)
{

    e.user.display (e.meat, "NOTICE", e.user.nick);
    
}

CIRCChannel.prototype.onCTCPAction =
function my_caction (e)
{

    e.user.display (e.CTCPData, "ACTION");

}

CIRCChannel.prototype.onUnknownCTCP =
function my_unkctcp (e)
{

    this.display ("Unknown CTCP " + e.CTCPCode + "(" + e.CTCPData +
                  ") from " + e.user.properNick, "BAD-CTCP");
    
}   

CIRCChannel.prototype.onJoin =
function my_cjoin (e)
{

    if (userIsMe (e.user))
    {
        this.display ("YOU have joined " + e.channel.name, "JOIN", "!ME");
        setCurrentObject(this);
    }
    else
        this.display(e.user.properNick + " (" + e.user.name + " @ " +
                     hyphenateWord(e.user.host, client.MAX_WORD_DISPLAY) +
                     ") has joined " + e.channel.name, "JOIN", e.user.nick);

    this._addUserToGraph (e.user);
    
    updateChannel (e.channel);
    
}

CIRCChannel.prototype.onPart =
function my_cpart (e)
{

    this._removeUserFromGraph(e.user);

    if (userIsMe (e.user))
    {
        this.display ("YOU have left " + e.channel.name, "PART", "!ME");
        if (client.currentObject == this)    
            /* hide the tree while we remove (possibly tons) of nodes */
            client.rdf.setTreeRoot("user-list", client.rdf.resNullChan);
        
        client.rdf.clearTargets(this.getGraphResource(),
                                client.rdf.resChanUser, true);

        if (client.currentObject == this)
            /* redisplay the tree */
            client.rdf.setTreeRoot("user-list", this.getGraphResource());
    }
    else
        this.display (e.user.properNick + " has left " + e.channel.name,
                      "PART");


    updateChannel (e.channel);
    
}

CIRCChannel.prototype.onKick =
function my_ckick (e)
{

    if (userIsMe (e.lamer))
        this.display ("YOU have been booted from " + e.channel.name +
                      " by " + e.user.properNick + " (" + e.reason + ")",
                      "KICK", e.user.nick);
    else
    {
        var enforcerProper, enforcerNick;
        if (userIsMe (e.user))
        {
            enforcerProper = "YOU";
            enforcerNick = "!ME";
        }
        else
        {
            enforcerProper = e.user.properNick;
            enforcerNick = e.user.nick;
        }
        
        this.display (e.lamer.properNick + " was booted from " +
                      e.channel.name + " by " + enforcerProper + " (" +
                      e.reason + ")", "KICK", enforcerNick);
    }
    
    this._removeUserFromGraph(e.lamer);

    updateChannel (e.channel);
    
}

CIRCChannel.prototype.onChanMode =
function my_cmode (e)
{

    if (e.user)
        this.display ("Mode " + e.params.slice(1).join(" ") + " by " +
                      e.user.nick, "MODE");

    for (var u in e.usersAffected)
        e.usersAffected[u].updateGraphResource();

    updateChannel (e.channel);
    updateTitle (e.channel);
    
}

    

CIRCChannel.prototype.onNick =
function my_cnick (e)
{

    if (userIsMe (e.user))
    {
        this.display ("YOU are now known as " + e.user.properNick, "NICK",
                      "!ME");
        updateNetwork();
    }
    else
        this.display (e.oldNick + " is now known as " + e.user.properNick,
                      "NICK");

    /*
      dd ("updating resource " + e.user.getGraphResource().Value +
        " to new nickname " + e.user.properNick);
    */

    e.user.updateGraphResource();
    
}


CIRCChannel.prototype.onQuit =
function my_cquit (e)
{

    if (userIsMe(e.user)) /* I dont think this can happen */
        this.display ("YOU have left " + e.server.parent.name +
                      " (" + e.reason + ")", "QUIT", "!ME");
    else
        this.display (e.user.properNick + " has left " + e.server.parent.name +
                      " (" + e.reason + ")", "QUIT");

    this._removeUserFromGraph(e.user);

    updateChannel (e.channel);
    
}

CIRCUser.prototype.onPrivmsg =
function my_cprivmsg (e)
{
    
    this.display (e.meat, "PRIVMSG");
    
}

CIRCUser.prototype.onNick =
function my_unick (e)
{

    if (userIsMe(e.user))
    {
        updateNetwork();
        updateTitle (e.channel);
    }
    
}

CIRCUser.prototype.onNotice =
function my_notice (e)
{

    this.display (e.meat, "NOTICE");
    
}

CIRCUser.prototype.onUnknownCTCP =
function my_unkctcp (e)
{

    this.parent.parent.display ("Unknown CTCP " + e.CTCPCode + "(" +
                                e.CTCPData + ") from " + e.user.properNick,
                                "BAD-CTCP");
    
}
