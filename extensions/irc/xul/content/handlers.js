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
 * The Original Code is ChatZilla
 *
 * The Initial Developer of the Original Code is New Dimensions Consulting,
 * Inc. Portions created by New Dimensions Consulting, Inc. are
 * Copyright (C) 1999 New Dimenstions Consulting, Inc. All
 * Rights Reserved.
 *
 * Contributor(s):
 *  Robert Ginda, rginda@netscape.com, original author
 *  Samuel Sieb, samuel@sieb.net
 *  Chiaki Koufugata chiaki@mozilla.gr.jp UI i18n 
 */

function onTopicEditStart()
{
    if (client.currentObject.TYPE != "IRCChannel")
        return;

    var text = client.statusBar["channel-topic"];
    var edit = client.statusBar["channel-topicedit"];
    text.setAttribute("collapsed", "true");
    edit.removeAttribute("collapsed");
    edit.value = client.currentObject.topic;
    edit.focus();
    edit.selectionStart = 0;
    edit.selectionEnd = edit.value.length;
}

function onTopicEditEnd ()
{
    if (!("statusBar" in client))
        return;
    
    var edit = client.statusBar["channel-topicedit"];
    var text = client.statusBar["channel-topic"];
    edit.setAttribute("collapsed", "true");
    text.removeAttribute("collapsed");
    focusInput();
}

function onTopicKeyPress (e)
{
    if (client.currentObject.TYPE != "IRCChannel")
        return;
    
    if (e.keyCode == 13)
    {        
        var line = stringTrim(e.target.value);
        client.currentObject.setTopic (fromUnicode(line));
        onTopicEditEnd();
    }
}

function onInputFocus ()
{
    if (!("statusBar" in client))
        return;
    
    var edit = client.statusBar["channel-topicedit"];
    var text = client.statusBar["channel-topic"];
    edit.setAttribute("collapsed", "true");
    text.removeAttribute("collapsed");
}

function onLoad()
{
    
    initHost(client);
    readIRCPrefs();
    setClientOutput();
    initStatic();

    client.userScripts = new Array();
    if (client.INITIAL_SCRIPTS)
    {
        var urls = client.INITIAL_SCRIPTS.split (";");
        for (var i = 0; i < urls.length; ++i)
        {
            client.userScripts[i] = new Object();
            client.load(stringTrim(urls[i]), client.userScripts[i]);
        }
    }
    
    processStartupURLs();
    mainStep();
}

function onClose()
{
    if ("userClose" in client && client.userClose)
        return true;
    
    client.userClose = true;
    client.currentObject.display (getMsg("cli_closing"), "INFO");

    if (client.getConnectionCount() == 0)
        /* if we're not connected to anything, just close the window */
        return true;

    /* otherwise, try to close out gracefully */
    client.quit (client.userAgent);
    return false;
}

function onUnload()
{
    if (client.SAVE_SETTINGS)
        writeIRCPrefs();
}

function onNotImplemented()
{

    alert (getMsg("onNotImplementedMsg"));
    
}

/* tab click */
function onTabClick (id)
{
    
    var tbi = document.getElementById (id);
    var view = client.viewsArray[tbi.getAttribute("viewKey")];

    setCurrentObject (view.source);
    
}

function onMouseOver (e)
{
    var i = 0;
    var target = e.target;
    var status = "";
    while (!status && target && i < 5)
    {
        if ("getAttribute" in target)
        {
            status = target.getAttribute("href");
            if (!status)
                status = target.getAttribute ("statusText");
        }
        ++i;
        target = target.parentNode;
    }
        
    if (status)
    {
        client.status = status;
    }
    else
    {
        if (client && "defaultStatus" in client)
            client.status = client.defaultStatus;
    }
}
    
function onSimulateCommand (line)
{
    onInputCompleteLine ({line: line}, true);
}

function onPopupSimulateCommand (line)
{
    if ("user" in client._popupContext)
    {
        var nick = client._popupContext.user;
        if (nick.indexOf("ME!") != -1)
        {
            var details = getObjectDetails(client.currentObject);
            if ("server" in details)
                nick = details.server.me.properNick;
        }

        line = line.replace (/\$nick/ig, nick);
    }
    
    onInputCompleteLine ({line: line}, true);
}

function onPopupHighlight (ruleText)
{
    var user = client._popupContext.user;
    
    var rec = findDynamicRule(".msg[msg-user=" + user + "]");
    
    if (!ruleText)
    {
        if (rec)
            rec.sheet.deleteRule(rec.index);
        /* XXX just deleting it doesn't work */
        addDynamicRule (".msg[msg-user=\"" + user + "\"] { }");
    }
    else
    {
        if (rec)
            rec.sheet.deleteRule(rec.index);

        addDynamicRule (".msg[msg-user=\"" + user + "\"] " +
                        "{" + ruleText + "}");
    }

}

function onToggleMungerEntry(entryName)
{
    client.munger.entries[entryName].enabled =
        !client.munger.entries[entryName].enabled;
    var item = document.getElementById("menu-munger-" + entryName);
    item.setAttribute ("checked", client.munger.entries[entryName].enabled);
}

function createPopupContext(event, target)
{
    var targetType;
    client._popupContext = new Object();
    client._popupContext.menu = event.originalTarget;

    if (!target)
        return "unknown";
    
    switch (target.tagName.toLowerCase())
    {
        case "html:a":
            var href = target.getAttribute("href");
            client._popupContext.url = href;
            if (href.indexOf("irc://") == 0)
            {
                var obj = parseIRCUrl(href);
                if (obj)
                {
                    if (obj.target)
                        if (obj.isnick)
                        {
                            targetType="nick-ircurl";
                            client._popupContext.user = obj.target;
                        }
                        else
                            targetType="channel-ircurl";
                    else
                        targetType="untargeted-ircurl";
                }
                else
                    targetType="weburl";
            }
            else
                targetType="weburl";
            break;
            
        case "html:td":
            var user = target.getAttribute("msg-user");
            if (user)
            {
                if (user.indexOf("ME!") != -1)
                    client._popupContext.user = "ME!";
                else
                    client._popupContext.user = user;
            }
            targetType = target.getAttribute("msg-type");
            break;            
    }

    client._popupContext.targetType = targetType;
    client._popupContext.targetClass = target.getAttribute("class");

    return targetType;
}

function onOutputContextMenuCreate(e)
{
    function evalIfAttribute (node, attr)
    {
        var expr = node.getAttribute(attr);
        if (!expr)
            return true;
        
        expr = expr.replace (/\Wor\W/gi, " || ");
        expr = expr.replace (/\Wand\W/gi, " && ");
        return eval("(" + expr + ")");
    }
        
    var target = document.popupNode;
    var foundSomethingUseful = false;
    
    do
    {
        if ("tagName" in target &&
            (target.tagName == "html:a" || target.tagName == "html:td"))
            foundSomethingUseful = true;
        else
            target = target.parentNode;
    } while (target && !foundSomethingUseful);
    
    var targetType = createPopupContext(e, target);
    var targetClass = ("targetClass" in client._popupContext) ?
        client._popupContext.targetClass : "";
    var viewType = client.currentObject.TYPE;
    var targetIsOp = "n/a";
    var targetIsVoice = "n/a";
    var iAmOp = "n/a";
    var targetUser = ("user" in client._popupContext) ?
        String(client._popupContext.user) : "";
    var details = getObjectDetails(client.currentObject);
    var targetServer = ("server" in details) ? details.server : "";

    if (targetServer && targetUser == "ME!")
    {
        targetUser = targetServer.me.nick;
    }

    var targetProperNick = targetUser;
    if (targetServer && targetUser in targetServer.users)
        targetProperNick = targetServer.users[targetUser].properNick;
    
    if (viewType == "IRCChannel" && targetUser)
    {
        if (targetUser in client.currentObject.users)
        {
            var cuser = client.currentObject.users[targetUser];
            targetIsOp = cuser.isOp ? "yes" : "no";
            targetIsVoice = cuser.isVoice ? "yes" : "no";
        }
        
        var server = getObjectDetails(client.currentObject).server;
        if (server &&
            server.me.nick in client.currentObject.users &&
            client.currentObject.users[server.me.nick].isOp)
        {
            iAmOp =  "yes";
        }
        else
        {
            iAmOp = "no";
        }
    }

    var popup = document.getElementById ("outputContext");
    var menuitem = popup.firstChild;

    do
    {
        if (evalIfAttribute(menuitem, "visibleif"))
        {
            menuitem.setAttribute ("hidden", "false");
        }
        else
        {
            menuitem.setAttribute ("hidden", "true");
            continue;
        }
        
        if (menuitem.hasAttribute("checkedif"))
        {
            if (evalIfAttribute(menuitem, "checkedif"))
                menuitem.setAttribute ("checked", "true");
            else
                menuitem.setAttribute ("checked", "false");
        }
            
        var format = menuitem.getAttribute("format");
        if (format)
        {
            format = format.replace (/\$nick/gi, targetProperNick);
            format = format.replace (/\$viewname/gi,
                                     client.currentObject.unicodeName);
            menuitem.setAttribute ("label", format);
        }
        
    } while ((menuitem = menuitem.nextSibling));

    return true;
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
    
    h.enabled = client.debugMode = !h.enabled;
    document.getElementById("menu-dmessages").setAttribute ("checked",
                                                            h.enabled);
    if (h.enabled)
        client.currentObject.display (getMsg("debug_on"), "INFO");
    else
        client.currentObject.display (getMsg("debug_off"), "INFO");
}

function onToggleSaveOnExit()
{
    client.SAVE_SETTINGS = !client.SAVE_SETTINGS;
    var m = document.getElementById ("menu-settings-autosave");
    m.setAttribute ("checked", String(client.SAVE_SETTINGS));

    var pref = Components.classes["@mozilla.org/preferences-service;1"]
                         .getService(Components.interfaces.nsIPrefBranch);
    pref.setBoolPref ("extensions.irc.settings.autoSave",
                      client.SAVE_SETTINGS);
}

function onToggleVisibility(thing)
{    
    var menu = document.getElementById("menu-view-" + thing);
    var ids = new Array();
    
    switch (thing)
    {
        case "tabstrip":
            ids = ["view-tabs"];
            break;
            
        case "info":
            ids = ["main-splitter", "user-list-box"];            
            break;
            
        case "header":
            ids = ["header-bar-tbox"];
            break;
            
        case "status":
            ids = ["status-bar"];
            break;

        default:
            dd ("** Unknown element ``" + menuId + 
                "'' passed to onToggleVisibility. **");
            return;
    }


    var newState;
    var elem = document.getElementById(ids[0]);
    var d = elem.getAttribute("collapsed");
    
    if (d == "true")
    {
        if (thing == "info")
        {
            if (client.currentObject.TYPE == "IRCChannel")
                client.rdf.setTreeRoot ("user-list", 
                                        client.currentObject.getGraphResource());
            else
                client.rdf.setTreeRoot ("user-list", client.rdf.resNullChan);
        }
        
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
    focusInput();
}

function onDoStyleChange (newStyle)
{

    if (newStyle == "other")
        newStyle = window.prompt (getMsg("onDoStyleChangeMsg"));

    if (newStyle)
    {
        setOutputStyle (newStyle);
        setCurrentObject(client.currentObject);
    }
    
}

function onHideView(view)
{
    var tb = getTabForObject(view);
    
    if (tb)
    {
        var i = deleteTab (tb);
        if (i != -1)
        {
            client.deck.removeChild(view.frame);
            delete view.frame;
            if (i >= client.viewsArray.length)
                i = client.viewsArray.length - 1;
            
            setCurrentObject (client.viewsArray[i].source);
        }
        
    }
}

function onDeleteView(view)
{
    var tb = getTabForObject(view);
    if (client.viewsArray.length < 2)
    {
        view.display (getMsg("onDeleteViewMsg"), "ERROR");
        return;
    }
    
    if (tb)
    {
        var i = deleteTab (tb);
        if (i != -1)
        {
            delete view.messageCount;
            delete view.messages;
            client.deck.removeChild(view.frame);
            delete view.frame;

            if (i >= client.viewsArray.length)
                i = client.viewsArray.length - 1;            
            setCurrentObject (client.viewsArray[i].source);
        }
        
    }
    
}

function onClearCurrentView()
{

    client.currentObject.messages = null;
    client.currentObject.messageCount = 0;

    client.currentObject.display (getMsg("onClearCurrentViewMsg"), "INFO");

    client.currentObject.frame.reload();
    
}

function onSortCol(sortColName)
{
    const nsIXULSortService = Components.interfaces.nsIXULSortService;
   /* XXX remove the rdf version once 0.8 is a distant memory
    * 2/22/2001
    */
    const isupports_uri =
        (Components.classes["@mozilla.org/xul/xul-sort-service;1"]) ?
         "@mozilla.org/xul/xul-sort-service;1" :
         "@mozilla.org/rdf/xul-sort-service;1";
    
    var node = document.getElementById(sortColName);
    if (!node)
        return false;
 
    // determine column resource to sort on
    var sortResource = node.getAttribute("resource");
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
    
    var currentDirection = node.getAttribute("sortDirection");
    
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
        xulSortService.sort(node, sortResource, sortDirection);
    }
    catch(ex)
    {
            //dd("Exception calling xulSortService.sort()");
    }
    
    return false;
}

function onToggleMunger()
{

    client.munger.enabled = !client.munger.enabled;
    var item = document.getElementById("menu-munger-global");
    item.setAttribute ("checked", !client.munger.enabled);

}

function onToggleColors()
{

    client.enableColors = !client.enableColors;
    document.getElementById("menu-colors").setAttribute ("checked",
                                                         client.enableColors);

}

function onMultilineInputKeyPress (e)
{
    if ((e.ctrlKey || e.metaKey) && e.keyCode == 13)
    {
        /* meta-enter, execute buffer */
        e.line = e.target.value;
        onInputCompleteLine (e);
    }
    else
    {
        if ((e.ctrlKey || e.metaKey) && e.keyCode == 40)
        {
            /* ctrl/meta-down, switch to single line mode */
            multilineInputMode (false);
        }
    }
}

function onToggleMsgCollapse()
{
    client.COLLAPSE_MSGS = !client.COLLAPSE_MSGS;
}

function onToggleCopyMessages()
{
    client.COPY_MESSAGES = !client.COPY_MESSAGES;
}

function onToggleStartupURL()
{
    var tb = getTabForObject (client.currentObject);
    if (!tb)
        return;
    
    var vk = Number(tb.getAttribute("viewKey"));
    
    var ary = client.INITIAL_URLS ? 
        client.INITIAL_URLS.split(/\s*;\s*/) : new Array();
    var url = client.currentObject.getURL();
    var index = arrayIndexOf(ary, url);
    if (index != -1)
        arrayRemoveAt(ary, index);
    else
        ary.push(url);
    
    client.INITIAL_URLS = ary.join ("; ");
}

function onViewMenuShowing ()
{
    var loc = client.currentFrame.document.location.href;
    loc = loc.substr (loc.indexOf("?") + 1);

    var val = (loc == "chrome://chatzilla/skin/output-default.css");
    document.getElementById ("menu-view-default").setAttribute ("checked", val);

    val = (loc == "chrome://chatzilla/skin/output-dark.css");
    document.getElementById ("menu-view-dark").setAttribute ("checked", val);

    val = (loc == "chrome://chatzilla/skin/output-light.css");
    document.getElementById ("menu-view-light").setAttribute ("checked", val);

    val = client.COLLAPSE_MSGS;
    document.getElementById ("menu-view-collapse").setAttribute ("checked", val);

    val = client.COPY_MESSAGES;
    document.getElementById ("menu-view-copymsgs").setAttribute ("checked", val);
    
    val = isStartupURL(client.currentObject.getURL());
    document.getElementById ("menu-view-startup").setAttribute ("checked", val);
    return true;
}
    
function onInputKeyPress (e)
{
    
    switch (e.keyCode)
    {        
        case 13: /* CR */
            e.line = stringTrim(e.target.value);
            if (!e.line)
                return;
            onInputCompleteLine (e);
            e.target.value = "";            
            break;

        case 38: /* up */
            if (e.ctrlKey || e.metaKey)
            {
                /* ctrl/meta-up, switch to multi line mode */
                multilineInputMode (true);
            }
            else
            {
                if (client.lastHistoryReferenced == -2)
                {
                    client.lastHistoryReferenced = -1;
                    e.target.value = client.incompleteLine;
                }
                else if (client.lastHistoryReferenced <
                         client.inputHistory.length - 1)
                {
                    e.target.value =
                        client.inputHistory[++client.lastHistoryReferenced];
                }
            }
            break;

        case 40: /* down */
            if (client.lastHistoryReferenced > 0)
                e.target.value =
                    client.inputHistory[--client.lastHistoryReferenced];
            else if (client.lastHistoryReferenced == -1)
            {
                e.target.value = "";
                client.lastHistoryReferenced = -2;
            }
            else
            {
                client.lastHistoryReferenced = -1;
                e.target.value = client.incompleteLine;
            }
            break;

        default:
            client.lastHistoryReferenced = -1;
            client.incompleteLine = e.target.value;
            break;
    }

}

function onTest ()
{

}

function onTabCompleteRequest (e)
{
    var elem = document.commandDispatcher.focusedElement;
    var singleInput = document.getElementById("input");
    if (document.getBindingParent(elem) != singleInput)
        return;

    e.preventDefault();
    
    var selStart = singleInput.selectionStart;
    var selEnd = singleInput.selectionEnd;            
    var line = singleInput.value;

    if (!line)
    {
        if ("defaultCompletion" in client.currentObject)
            singleInput.value = client.currentObject.defaultCompletion;
        return;
    }
    
    if (selStart != selEnd) 
    {
        /* text is highlighted, just move caret to end and exit */
        singleInput.selectionStart = singleInput.selectionEnd = line.length;
        return;
    }

    var wordStart = line.substr(0, selStart).search(/\s\S*$/);
    if (wordStart == -1)
        wordStart = 0;
    else
        ++wordStart;
    
    var wordEnd = line.substr(selStart).search(/\s/);
    if (wordEnd == -1)
        wordEnd = line.length;
    else
        wordEnd += selStart;

    if ("performTabMatch" in client.currentObject)
    {
        var word = line.substring (wordStart, wordEnd);
        var matches = client.currentObject.performTabMatch (line, wordStart,
                                                            wordEnd,
                                                            word.toLowerCase(),
                                                            selStart);
        /* if we get null back, we're supposed to fail silently */
        if (!matches)
            return;

        var doubleTab = false;
        var date = new Date();
        if ((date - client.lastTabUp) <= client.DOUBLETAB_TIME)
            doubleTab = true;
        else
            client.lastTabUp = date;

        if (doubleTab)
        {
            /* if the user hit tab twice quickly, */
            if (matches.length > 0)
            {
                /* then list possible completions, */
                client.currentObject.display(getMsg("tabCompleteList",
                                                    [matches.length, word,
                                                     matches.join(MSG_CSP)]),
                                             "INFO");
            }
            else
            {
                /* or display an error if there are none. */
                client.currentObject.display(getMsg("tabCompleteError", [word]),
                                             "ERROR");
            }
        }
        else if (matches.length >= 1)
        {
            var match;
            if (matches.length == 1)
                match = matches[0];
            else
                match = getCommonPfx(matches);
            singleInput.value = line.substr(0, wordStart) + match + 
                    line.substr(wordEnd);
            if (wordEnd < line.length)
            {
                /* if the word we completed was in the middle if the line
                 * then move the cursor to the end of the completed word. */
                var newpos = wordStart + match.length;
                if (matches.length == 1)
                {
                    /* word was fully completed, move one additional space */
                    ++newpos;
                }
                singleInput.selectionEnd = e.target.selectionStart = newpos;
            }
        }
    }

}

function onWindowKeyPress (e)
{
    var code = Number (e.keyCode);
    var w;
    var newOfs;
    
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
            if ((idx in client.viewsArray) && (client.viewsArray[idx].source))
                setCurrentObject(client.viewsArray[idx].source);
            break;

        case 33: /* pgup */
            w = client.currentFrame;
            newOfs = w.pageYOffset - (w.innerHeight / 2);
            if (newOfs > 0)
                w.scrollTo (w.pageXOffset, newOfs);
            else
                w.scrollTo (w.pageXOffset, 0);
            e.preventDefault();
            break;
            
        case 34: /* pgdn */
            w = client.currentFrame;
            newOfs = w.pageYOffset + (w.innerHeight / 2);
            if (newOfs < (w.innerHeight + w.pageYOffset))
                w.scrollTo (w.pageXOffset, newOfs);
            else
                w.scrollTo (w.pageXOffset, (w.innerHeight + w.pageYOffset));
            e.preventDefault();
            break;

        case 9: /* tab */
            e.preventDefault();
            break;
            
        default:
            
    }

}

function onInputCompleteLine(e, simulated)
{

    if (!simulated)
    {
        if (client.inputHistory[0] != e.line)
            client.inputHistory.unshift (e.line);
    
        if (client.inputHistory.length > client.MAX_HISTORY)
            client.inputHistory.pop();
        
        client.lastHistoryReferenced = -1;
        client.incompleteLine = "";
    }
    
    if (e.line[0] == client.COMMAND_CHAR)
    {
        var ary = e.line.substr(1, e.line.length).match (/(\S+)? ?(.*)/);
        var command = ary[1].toLowerCase();
        
        var ev = new CEvent ("client", "input-command", client,
                             "onInputCommand");
        ev.command = command;
        ev.inputData =  ary[2] ? stringTrim(ary[2]) : "";
        ev.line = e.line;
        
        ev.target = client.currentObject;
        getObjectDetails (ev.target, ev);
        client.eventPump.addEvent (ev);
    }
    else /* plain text */
    {
        client.sayToCurrentTarget (e.line);
    }
}

function onNotifyTimeout ()
{
    for (var n in client.networks)
    {
        var net = client.networks[n];
        if (net.isConnected()) {
            if ("notifyList" in net && net.notifyList.length > 0) {
                net.primServ.sendData ("ISON " +
                                       client.networks[n].notifyList.join(" ")
                                       + "\n");
            } else {
                /* if the notify list is empty, just send a ping to see if we're
                 * alive. */
                net.primServ.sendData ("PING :ALIVECHECK\n");
            }
        }
    }
}
    
client.onInputCancel =
function cli_icancel (e)
{
    if (client.currentObject.TYPE != "IRCNetwork")
    {
        client.currentObject.display (getMsg("cli_icancelMsg"), "ERROR");
        return false;
    }
    
    if (!client.currentObject.connecting)
    {
        client.currentObject.display (getMsg("cli_icancelMsg2"), "ERROR");
        return false;
    }
    
    client.currentObject.connectAttempt = 
        client.currentObject.MAX_CONNECT_ATTEMPTS + 1;

    client.currentObject.display (getMsg("cli_icancelMsg3",
                                         client.currentObject.name), "INFO");

    return true;
}    

client.onInputCharset =
function cli_icharset (e)
{
    if (e.inputData)
    {
        if (!setCharset(e.inputData))
        {
            client.currentObject.display (getMsg("cli_charsetError",
                                                 e.inputData),
                                          "ERROR");
            return false;
        }
	}

    client.currentObject.display (getMsg("cli_currentCharset", client.CHARSET),
                                  "INFO");
    return true;
}

client.onInputCommand = 
function cli_icommand (e)
{
    var ary = client.commands.list (e.command);
    
    if (ary.length == 0)
    {        
        var o = getObjectDetails(client.currentObject);
        if ("server" in o)
        {
            client.currentObject.display (getMsg("cli_icommandMsg",
                                                 e.command), "WARNING");
            o.server.sendData (e.command + " " + e.inputData + "\n");
        }
        else
            client.currentObject.display (getMsg("cli_icommandMsg2",
                                                 e.command), "ERROR");
    }
    else if (ary.length == 1 || ary[0].name == e.command)
    {
        if (typeof client[ary[0].func] == "undefined")        
            client.currentObject.display (getMsg("cli_icommandMsg3",
                                                 ary[0].name), "ERROR");
        else
        {
            e.commandEntry = ary[0];
            if (!client[ary[0].func](e))
                client.currentObject.display (ary[0].name + " " +
                                              ary[0].usage, "USAGE");
        }
    }
    else
    {
        client.currentObject.display (getMsg("cli_icommandMsg4",
                                             e.command), "ERROR");
        var str = "";
        for (var i in ary)
            str += str ? ", " + ary[i].name : ary[i].name;
        client.currentObject.display (getMsg("cli_icommandMsg5",
                                             [ary.length, str]), "ERROR");
    }

}

client.onInputCSS =
function cli_icss (e)
{
    if (e.inputData)
    {
        e.inputData = stringTrim(e.inputData);
    
        if (e.inputData.search(/^light$/i) != -1)
            e.inputData = "chrome://chatzilla/skin/output-light.css";
        else if (e.inputData.search(/^dark$/i) != -1)
            e.inputData = "chrome://chatzilla/skin/output-dark.css";
        else if (e.inputData.search(/^default$/i) != -1)
            e.inputData = "chrome://chatzilla/skin/output-default.css";
        else if (e.inputData.search(/^none$/i) != -1)
            e.inputData = "chrome://chatzilla/content/output-base.css";
    
        for (var i = 0; i < client.deck.childNodes.length; i++)
        {
            client.deck.childNodes[i].loadURI (
                "chrome://chatzilla/content/outputwindow.html?" + e.inputData);
        }
        client.DEFAULT_STYLE = e.inputData;
    }
    
    client.currentObject.display (getMsg("cli_icss", client.DEFAULT_STYLE),
                                  "INFO");
    return true;
}

client.onInputSimpleCommand =
function cli_iscommand (e)
{    
    var o = getObjectDetails(client.currentObject);
    
    if ("server" in o)
    {
        o.server.sendData (e.command + " " + e.inputData + "\n");
        return true;
    }
    else
    {
        client.currentObject.display (getMsg("onInputSimpleCommandMsg",
                                             e.command), "WARNING");
        return false;
    }
}

client.onInputSquery =
function cli_isquery (e)
{
    var o = getObjectDetails(client.currentObject);
    
    if ("server" in o)
    {
        var ary = e.inputData.match (/(\S+)? ?(.*)/);
        if (ary == null)
            return false;

        if (ary.length == 1)
        {
            o.server.sendData ("SQUERY " + ary[1] + "\n");
        }
        else
        {
            o.server.sendData ("SQUERY " + ary[1] + " :" + ary[2] + "\n");
        }
        return true;
    }
    else
    {
        client.currentObject.display (getMsg("onInputSimpleCommandMsg",
                                             e.command), "WARNING");
        return false;
    }
}

client.onInputStatus =
function cli_istatus (e)
{    
    function serverStatus (s)
    {
        if (!s.connection.isConnected)
        {
            client.currentObject.display(getMsg("cli_istatusServerOff",
                                                s.parent.name), "STATUS");
            return;
        }
        
        var serverType = (s.parent.primServ == s) ?
            getMsg("cli_istatusPrimary") : getMsg("cli_istatusSecondary");
        client.currentObject.display(getMsg("cli_istatusServerOn",
                                            [s.parent.name, s.me.properNick,
                                             s.connection.host,
                                             s.connection.port, serverType]),
                                     "STATUS");        

        var connectTime = formatDateOffset (Math.floor((new Date() -
                                            s.connection.connectDate) / 1000));
        var pingTime = ("lastPing" in s) ?
            formatDateOffset (Math.floor((new Date() - s.lastPing) / 1000)) :
            getMsg("na");
        var lag = (s.lag >= 0) ? s.lag : getMsg("na");

        client.currentObject.display(getMsg("cli_istatusServerDetail", 
                                            [s.parent.name, connectTime,
                                             pingTime, lag]), "STATUS");
    }

    function channelStatus (c)
    {
        var cu;
        var net = c.parent.parent.name;

        if ((cu = c.users[c.parent.me.nick]))
        {
            var mtype;
            
            if (cu.isOp && cu.isVoice)
                mtype = getMsg("cli_istatusBoth");
            else if (cu.isOp)
                mtype = getMsg("cli_istatusOperator");
            else if (cu.isVoice)
                mtype = getMsg("cli_istatusVoiced");
            else
                mtype = getMsg("cli_istatusMember");

            var mode = c.mode.getModeStr();
            if (!mode)
                mode = getMsg("cli_istatusNoMode");
            
            client.currentObject.display (getMsg("cli_istatusChannelOn",
                                                 [net, mtype, c.unicodeName,
                                                  mode,
                                                  "irc://" + escape(net) + "/" +
                                                  escape(c.encodedName) + "/"]),
                                          "STATUS");
            client.currentObject.display (getMsg("cli_istatusChannelDetail",
                                                 [net, c.unicodeName,
                                                  c.getUsersLength(),
                                                  c.opCount, c.voiceCount]),
                                          "STATUS");
            if (c.topic)
                client.currentObject.display (getMsg("cli_istatusChannelTopic",
                                              [net, c.unicodeName, c.topic]),
                                              "STATUS");
            else
                client.currentObject.display (getMsg("cli_istatusChannelNoTopic",
                                              [net, c.unicodeName]), "STATUS");
        }
        else
            client.currentObject.display (getMsg("cli_istatusChannelOff",
                                          [net, c.unicodeName]), "STATUS");
    }

    client.currentObject.display (client.userAgent, "STATUS");
    client.currentObject.display (getMsg("cli_istatusClient",
                                         [CIRCNetwork.prototype.INITIAL_NICK,
                                         CIRCNetwork.prototype.INITIAL_NAME,
                                         CIRCNetwork.prototype.INITIAL_DESC]),
                                  "STATUS");
        
    var n, s, c;

    switch (client.currentObject.TYPE)
    {
        case "IRCNetwork":
            for (s in client.currentObject.servers)
            {
                serverStatus(client.currentObject.servers[s]);
                for (c in client.currentObject.servers[s].channels)
                    channelStatus (client.currentObject.servers[s].channels[c]);
            }
            break;

        case "IRCChannel":
            serverStatus(client.currentObject.parent);
            channelStatus(client.currentObject);
            break;
            
        default:
            for (n in client.networks)
                for (s in client.networks[n].servers)
                {
                    serverStatus(client.networks[n].servers[s]);
                    for (c in client.networks[n].servers[s].channels)
                        channelStatus (client.networks[n].servers[s].channels[c]);
                }
            break;
    }

    client.currentObject.display (getMsg("cli_istatusEnd"), "END_STATUS");
    return true;
    
}            
            
client.onInputHelp =
function cli_ihelp (e)
{
    var ary = client.commands.list (e.inputData);
 
    if (ary.length == 0)
    {
        client.currentObject.display (getMsg("cli_ihelpMsg", e.inputData),
                                      "ERROR");
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
    var o = getObjectDetails(client.currentObject);
    
    client.currentObject.display (getMsg("cli_testdisplayMsg"), "HELLO");
    client.currentObject.display (getMsg("cli_testdisplayMsg2"), "INFO");
    client.currentObject.display (getMsg("cli_testdisplayMsg3"), "ERROR");
    client.currentObject.display (getMsg("cli_testdisplayMsg4"), "HELP");
    client.currentObject.display (getMsg("cli_testdisplayMsg5"), "USAGE");
    client.currentObject.display (getMsg("cli_testdisplayMsg6"), "STATUS");

    if (o.server && o.server.me)
    {
        var me = o.server.me;
        var viewType = client.currentObject.TYPE;
        var sampleUser = {TYPE: "IRCUser", nick: "ircmonkey",
                          name: "IRCMonkey", properNick: "IRCMonkey",
                          host: ""};
        var sampleChannel = {TYPE: "IRCChannel", name: "#mojo"};

        function test (from, to)
        {
            var fromText = (from != me) ? from.TYPE + " ``" + from.name + "''" :
                getMsg("cli_testdisplayYou");
            var toText   = (to != me) ? to.TYPE + " ``" + to.name + "''" :
                getMsg("cli_testdisplayYou");
            
            client.currentObject.display (getMsg("cli_testdisplayMsg7",
                                                 [fromText, toText]),
                                          "PRIVMSG", from, to);
            client.currentObject.display (getMsg("cli_testdisplayMsg8",
                                                 [fromText, toText]),
                                          "ACTION", from, to);
            client.currentObject.display (getMsg("cli_testdisplayMsg9",
                                                 [fromText, toText]),
                                          "NOTICE", from, to);
        }
        
        test (sampleUser, me); /* from user to me */
        test (me, sampleUser); /* me to user */

        client.currentObject.display (getMsg("cli_testdisplayMsg10"),
                                      "PRIVMSG", sampleUser, me);
        client.currentObject.display (getMsg("cli_testdisplayMsg11"),
                                      "PRIVMSG", sampleUser, me);
        client.currentObject.display (getMsg("cli_testdisplayMsg12"),
                                      "PRIVMSG", sampleUser, me);
        client.currentObject.display (getMsg("cli_testdisplayMsg13"),
                                      "PRIVMSG", sampleUser, me);
        client.currentObject.display (unescape(getMsg("cli_testdisplayMsg20")),
                                      "PRIVMSG", sampleUser, me);
        client.currentObject.display (unescape(getMsg("cli_testdisplayMsg21")),
                                      "PRIVMSG", sampleUser, me);
        

        if (viewType == "IRCChannel")
        {
            test (sampleUser, sampleChannel); /* user to channel */
            test (me, sampleChannel);         /* me to channel */
            client.currentObject.display (getMsg("cli_testdisplayMsg14"),
                                          "TOPIC", sampleUser, sampleChannel);
            client.currentObject.display (getMsg("cli_testdisplayMsg15"), "JOIN",
                                          sampleUser, sampleChannel);
            client.currentObject.display (getMsg("cli_testdisplayMsg16"), "PART",
                                          sampleUser, sampleChannel);
            client.currentObject.display (getMsg("cli_testdisplayMsg17"), "KICK",
                                          sampleUser, sampleChannel);
            client.currentObject.display (getMsg("cli_testdisplayMsg18"), "QUIT",
                                          sampleUser, sampleChannel);
            client.currentObject.display (getMsg("cli_testdisplayMsg19",
                                                 me.nick),
                                          "PRIVMSG", sampleUser, sampleChannel);
            client.currentObject.display (getMsg("cli_testdisplayMsg11"),
                                          "PRIVMSG", me, sampleChannel);
        }        
        
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
        setCurrentObject (net);    
    }
    else
    {
        client.currentObject.display (getMsg("cli_inetworkMsg",e.inputData),
                                      "ERROR");
        return false;
    }
    
    return true;
    
}

client.onInputNetworks =
function clie_ilistnets (e)
{
    var span = document.createElementNS("http://www.w3.org/1999/xhtml",
                                        "html:span");
    
    span.appendChild (newInlineText(getMsg("cli_listNetworks.a")));

    var netnames = keys(client.networks).sort();
    var lastname = netnames[netnames.length - 1];
    
    for (n in netnames)
    {
        var net = client.networks[netnames[n]];
        var a = document.createElementNS("http://www.w3.org/1999/xhtml",
                                         "html:a");
        a.setAttribute ("class", "chatzilla-link");
        a.setAttribute ("href", "irc://" + net.name);
        var t = newInlineText (net.name);
        a.appendChild (t);
        span.appendChild (a);
        if (netnames[n] != lastname)
            span.appendChild (newInlineText (MSG_CSP));
    }

    span.appendChild (newInlineText(getMsg("cli_listNetworks.b")));

    client.currentObject.display (span, "INFO");
    return true;
}   

client.onInputServer =
function cli_iserver (e)
{
    if (!e.inputData)
        return false;

    var ary = 
        e.inputData.match(/^([^\s\:]+)[\s\:]?(?:(\d+)(?:\s+(\S+)|\s*$)|(\S+)|$)/);
    var pass;
    if (3 in ary && ary[3])
        pass = ary[3];
    else if (4 in ary && ary[4])
        pass = ary[4];
    
    if (ary == null)
        return false;

    var port;
    if (2 in ary && ary[2])
        port = ary[2];
    else
        port = 6667;

    var net = null;
    
    for (var n in client.networks)
    {
        if (n == ary[1])
        {
            if (client.networks[n].isConnected())
            {
                client.currentObject.display (getMsg("cli_iserverMsg",ary[1]),
                                              "ERROR");
                return false;
            }
            else
            {
                net = client.networks[n];
                break;
            }
        }
    }

    if (!net)
    {
        /* if there wasn't already a network created for this server,
         * make one. */
        client.networks[ary[1]] =
            new CIRCNetwork (ary[1],
                             [{name: ary[1], port: port, password: pass}],
                             client.eventPump);
    }
    else
    {
        /* if we've already created a network for this server, reinit the
         * serverList, in case the user changed the port or password */
        net.serverList = [{name: ary[1], port: port, password: pass}];
    }

    client.connectToNetwork (ary[1]);
    return true;

}

client.onInputQuit =
function cli_quit (e)
{
    if (!("server" in e))
    {
        client.currentObject.display (getMsg("cli_quitMsg"), "ERROR");
        return false;
    }

    if (!e.server.connection.isConnected)
    {
        client.currentObject.display (getMsg("cli_quitMsg2"), "ERROR");
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
    return true;
}

client.onInputDelete =
function cli_idelete (e)
{
    if ("inputData" in e && e.inputData)
        return false;
    onDeleteView(client.currentObject);
    return true;

}

client.onInputHide=
function cli_ihide (e)
{
    
    onHideView(client.currentObject);
    return true;

}

client.onInputClear =
function cli_iclear (e)
{
    
    onClearCurrentView();
    return true;

}

client.onInputNames =
function cli_inames (e)
{
    var chan;
    
    if (!e.network)
    {
        client.currentObject.display (getMsg("cli_inamesMsg"), "ERROR");
        return false;
    }

    if (e.inputData)
    {
        if (!e.network.isConnected())
        {
            client.currentObject.display (getMsg("cli_inamesMsg2",
                                                 e.network.name), "ERROR");
            return false;
        }

        var encodeName = fromUnicode(e.inputData + " ");
        encodeName = encodeName.substr(0, encodeName.length -1);
        chan = encodeName;

    }
    else
    {
        if (client.currentObject.TYPE != "IRCChannel")
        {
            client.currentObject.display (getMsg("cli_inamesMsg3"),"ERROR");
            return false;
        }
        
        chan = e.channel.name;
    }
    
    client.currentObject.pendingNamesReply = true;
    e.server.sendData ("NAMES " + chan + "\n");
    
    return true;
    
}

client.onInputInfobar =
function cli_tinfo ()
{
    
    onToggleVisibility ("info");
    return true;
    
}

client.onInputTabstrip =
function cli_itabs ()
{
    
    onToggleVisibility ("tabstrip");
    return true;

}

client.onInputStatusbar =
function cli_isbar ()
{
    
    onToggleVisibility ("status");
    return true;

}

client.onInputHeaders =
function cli_isbar ()
{
    
    onToggleVisibility ("header");
    return true;

}

client.onInputCommands =
function cli_icommands (e)
{

    client.currentObject.display (getMsg("cli_icommandsMsg"), "INFO");
    
    var pattern = (e && e.inputData) ? e.inputData : "";
    var matchResult = client.commands.listNames(pattern).join(MSG_CSP);
    
    if (pattern)
        client.currentObject.display (getMsg("cli_icommandsMsg2a",
                                             [pattern, matchResult]), "INFO");
    else
        client.currentObject.display (getMsg("cli_icommandsMsg2b", matchResult),
                                      "INFO");

    return true;
}

client.onInputAttach =
function cli_iattach (e)
{
    if (!e.inputData)
    {
        return false;
    }

    if (e.inputData.search(/irc:\/\//i) != 0)
        e.inputData = "irc://" + e.inputData;
    
    var url = parseIRCURL(e.inputData);
    if (!url)
    {
        client.currentObject.display (getMsg("badIRCURL", e.inputData),
                                      "ERROR");
        return false;
    }
    
    gotoIRCURL (url);
    return true;
}
    
client.onInputMe =
function cli_ime (e)
{
    if (typeof client.currentObject.act != "function")
    {
        client.currentObject.display (getMsg("cli_imeMsg"), "ERROR");
        return false;
    }

    e.inputData = filterOutput (e.inputData, "ACTION", "ME!");
    client.currentObject.display (e.inputData, "ACTION", "ME!",
                                  client.currentObject);
    client.currentObject.act (fromUnicode(e.inputData));
    
    return true;
}

client.onInputList =
function cli_ilist (e)
{
    var o = getObjectDetails(client.currentObject);
    if ("network" in o)
    {
        o.network.list = new Array();
        o.network.list.regexp = null;
    }
    return client.onInputSimpleCommand(e);
}

client.onInputRlist =
function cli_irlist (e)
{
    var o = getObjectDetails(client.currentObject);

    if (!("server" in o))
    {
        client.currentObject.display (getMsg("onInputSimpleCommandMsg",
                                             "list"), "WARNING");
        return false;
    }
    o.network.list = new Array();
    var ary = e.inputData.match (/^\s*("([^"]*)"|([^\s]*))/);
    try
    {
        if (ary[2])
            o.network.list.regexp = new RegExp(ary[2], "i");
        else if (ary[3])
            o.network.list.regexp = new RegExp(ary[3], "i");
        else
            return false;
    }
    catch(error)
    {
        client.currentObject.display (getMsg("cli_irlistMsg", e.inputData,
                                             error),
                                      "ERROR");
        return false;
    }
    o.server.sendData ("list\n");
    return true;
}

client.onInputQuery =
function cli_iquery (e)
{
    if (!e.server.users)
    {
        client.currentObject.display (getMsg("cli_imsgMsg"), "ERROR");
        return false;
    }

    var ary = e.inputData.match (/(\S+)\s*(.*)?/);
    if (ary == null)
    {
        client.currentObject.display (getMsg("cli_imsgMsg2"), "ERROR");
        return false;
    }

    var tab = openQueryTab (e.server, ary[1]);
    setCurrentObject (tab);

    if (ary[2])
    {
        var msg = filterOutput(ary[2], "PRIVMSG", "ME!");
        tab.display (msg, "PRIVMSG", "ME!", tab);
        tab.say (fromUnicode(ary[2]));
    }
    
    e.user = tab;

    return true;
}

client.onInputMsg =
function cli_imsg (e)
{

    if (!e.network || !e.network.isConnected())
    {
        client.currentObject.display (getMsg("cli_imsgMsg4"), "ERROR");
        return false;
    }

    var ary = e.inputData.match (/(\S+)\s+(.*)/);
    if (ary == null)
        return false;

    var usr = e.network.primServ.addUser(ary[1].toLowerCase());

    var msg = filterOutput(ary[2], "PRIVMSG", "ME!");
    client.currentObject.display (msg, "PRIVMSG", "ME!", usr);
    usr.say (fromUnicode(ary[2]));

    return true;

}

client.onInputNick =
function cli_inick (e)
{

    if (!e.inputData)
        return false;
    
    if (e.server) 
        e.server.sendData ("NICK " + e.inputData + "\n");

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
        client.currentObject.display (getMsg("cli_iquoteMsg"), "ERROR");
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
        client.currentObject.doEval = function (__s) { return eval(__s); }
        client.currentObject.display (e.inputData, "EVAL-IN");
        
        var rv = String(client.currentObject.doEval (e.inputData));
        
        client.currentObject.display (rv, "EVAL-OUT");

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
        client.currentObject.display (getMsg("cli_ictcpMsg"), "ERROR");
        return false;
    }

    var ary = e.inputData.match(/^(\S+) (\S+)\s?(.*)$/);
    if (ary == null)
        return false;
    
    e.server.ctcpTo (ary[1], ary[2], ary[3]);
    
    return true;
    
}


client.onInputJoin =
function cli_ijoin (e)
{
    if (!e.network || !e.network.isConnected())
    {
        if (!e.network)
            client.currentObject.display (getMsg("cli_ijoinMsg"), "ERROR");
        else
            client.currentObject.display (getMsg("cli_ijoinMsg2",
                                                 e.network.name1), "ERROR");
        return false;
    }
    
    var ary = e.inputData.match(/(((\S+), *)*(\S+)) *(\S+)?/);
    var name;
    var key = "";
    var namelist;

    if (ary)
    {
        namelist = ary[1].split(/, */);
        if (5 in ary)
            key = ary[5];
    }
    else
    {
        if (client.currentObject.TYPE == "IRCChannel")
            namelist = [client.currentObject.name];
        else
            return false;

        if (client.currentObject.mode.key)
            key = client.currentObject.mode.key
    }

    for (i in namelist)
    {
        name = namelist[i];
        if ((name[0] != "#") && (name[0] != "&") && (name[0] != "+") &&
            (name[0] != "!"))
            name = "#" + name;

        e.channel = e.server.addChannel (name);
        e.channel.join(key);
        if (!("messages" in e.channel))
        {
            e.channel.display (getMsg("cli_ijoinMsg3", e.channel.unicodeName),
                               "INFO");
        }
        setCurrentObject(e.channel);
    }

    return true;
}

client.onInputLeave =
function cli_ipart (e)
{
    if (!e.channel)
    {            
        client.currentObject.display (getMsg("cli_ipartMsg"), "ERROR");
        return false;
    }

    e.channel.part();

    return true;
    
}

client.onInputZoom =
function cli_izoom (e)
{
    client.currentObject.display (getMsg("cli_izoomMsg"), "WARNING");

    if (!e.inputData)
        return false;
    
    if (!e.channel)
    {
        client.currentObject.display (getMsg("cli_izoomMsg2"), "ERROR");
        return false;
    }
    
    var cuser = e.channel.getUser(e.inputData);
    
    if (!cuser)
    {
        client.currentObject.display (getMsg("cli_izoomMsg3",e.inputData),
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
        client.currentObject.display (getMsg("cli_whoisMsg"), "ERROR");
        return false;
    }

    if (!e.inputData)
    {
        var nicksAry = e.channel.getSelectedUsers();
 
        if (nicksAry)
        {
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
        client.currentObject.display (getMsg("cli_itopicMsg"), "ERROR");
        return false;
    }
    
    if (!e.inputData)
    {
        e.server.sendData ("TOPIC " + e.channel.name + "\n");
    }
    else
    {
        if (!e.channel.setTopic(fromUnicode(e.inputData)))
            client.currentObject.display (getMsg("cli_itopicMsg2"), "ERROR");
    }

    return true;
    
}

client.onInputAbout =
function cli_iabout (e)
{
    client.currentObject.display (CIRCServer.prototype.VERSION_RPLY, "ABOUT");
    client.currentObject.display (getMsg("aboutHomepage"), "ABOUT");
    return true;
}

client.onInputAway =
function cli_iaway (e)
{ 
    if (!e.network || !e.network.isConnected())
    {
        client.currentObject.display (getMsg("cli_iawayMsg"), "ERROR");
        return false;
    }
    else if (!e.inputData) 
    {
        e.server.sendData ("AWAY\n");
    }
    else
    {
        e.server.sendData ("AWAY :" + e.inputData + "\n");
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
        client.currentObject.display (getMsg("cli_ideopMsg"), "ERROR");
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
        client.currentObject.display (getMsg("cli_ideopMsg2",e.inputData),
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
        client.currentObject.display (getMsg("cli_iopMsg"), "ERROR");
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
        client.currentObject.display (getMsg("cli_iopMsg2",e.inputData),"ERROR");
        return false;
    }
    
    cuser.setOp(true);

    return true;   
    
}

client.onInputPing = 
function cli_iping (e) 
{
    if (!e.inputData)
        return false;
    
    onSimulateCommand("/ctcp " + e.inputData + " PING");
    return true;
}

client.onInputVersion = 
function cli_iversion (e) 
{
    if (!e.inputData)
        return false;
    
    onSimulateCommand("/ctcp " + e.inputData + " VERSION");
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
        client.currentObject.display (getMsg("cli_ivoiceMsg"), "ERROR");
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
        client.currentObject.display (getMsg("cli_ivoiceMsg2",e.inputData),
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
        client.currentObject.display (getMsg("cli_devoiceMsg"), "ERROR");
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
        client.currentObject.display (getMsg("cli_devoiceMsg2", e.inputData),
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
        client.currentObject.display (getMsg("cli_iinviteMsg"), "ERROR");
        return false;
    }     
    else if (!e.channel)
    {
        client.currentObject.display (getMsg("cli_iinviteMsg2"), "ERROR");
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
            var encodeName = fromUnicode(ary[1] + " ");
            encodeName = encodeName.substr(0, encodeName.length -1);
            var chan = e.server.channels[encodeName.toLowerCase()];
             
            if (chan == undefined) 
            {
                client.currentObject.display (getMsg("cli_iinviteMsg3", ary[1]),
                                              "ERROR");
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
        client.currentObject.display (getMsg("cli_ikickMsg"), "ERROR");
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
        client.currentObject.display (getMsg("cli_ikickMsg2", e.inputData),
                                      "ERROR");
        return false;
    }

    if (ary.length > 2)
    {               
        cuser.kick(fromUnicode(ary[2]));
    }
    else     

    cuser.kick();    
     
    return true;
}

client.onInputClient =
function cli_iclient (e)
{
    if (!client.messages)
        client.display (getMsg("cli_iclientMsg"), "INFO");
    getTabForObject (client, true);
    setCurrentObject (client);
    return true;
}

client.onInputNotify =
function cli_inotify (e)
{
    if (!("network" in e) || !e.network)
    {
        client.currentObject.display (getMsg("cli_inotifyMsg"), "ERROR");
        return false;
    }

    var net = e.network;
    
    if (!e.inputData)
    {
        if ("notifyList" in net && net.notifyList.length > 0)
        {
            /* delete the lists and force a ISON check, this will
             * print the current online/offline status when the server
             * responds */
            delete net.onList;
            delete net.offList;
            onNotifyTimeout();
        }
        else
            client.currentObject.display (getMsg("cli_inotifyMsg2"), "INFO");
    }
    else
    {
        var adds = new Array();
        var subs = new Array();
        
        if (!("notifyList" in net))
            net.notifyList = new Array();
        var ary = e.inputData.toLowerCase().split(/\s+/);

        for (var i in ary)
        {
            var idx = arrayIndexOf (net.notifyList, ary[i]);
            if (idx == -1)
            {
                net.notifyList.push (ary[i]);
                adds.push(ary[i]);
            }
            else
            {
                arrayRemoveAt (net.notifyList, idx);
                subs.push(ary[i]);
            }
        }

        var msgname;
        
        if (adds.length > 0)
        {
            msgname = (adds.length == 1) ? "cli_inotifyMsg3a" : 
                                           "cli_inotifyMsg3b";
            client.currentObject.display (getMsg(msgname, arraySpeak(adds)));
        }
        
        if (subs.length > 0)
        {
            msgname = (subs.length == 1) ? "cli_inotifyMsg4a" : 
                                           "cli_inotifyMsg4b";
            client.currentObject.display (getMsg(msgname, arraySpeak(subs)));
        }
            
        delete net.onList;
        delete net.offList;
        onNotifyTimeout();
    }

    return true;
    
}

                
client.onInputStalk =
function cli_istalk (e)
{
    if (!e.inputData)
    {
        if (client.stalkingVictims.length == 0)
        {
            client.currentObject.display(getMsg("cli_istalkMsg"), "STALK");
        }
        else
        {
            client.currentObject.display
                (getMsg("cli_istalkMsg2", client.stalkingVictims.join(MSG_CSP)),
                 "STALK");
        }
        return true;
    }

    client.stalkingVictims[client.stalkingVictims.length] = e.inputData;
    client.currentObject.display(getMsg("cli_istalkMsg3",e.inputData), "STALK");
    return true;
}

client.onInputUnstalk =
function cli_iunstalk ( e )
{
    if (!e.inputData)
        return false;

    for (i in client.stalkingVictims)
    {
        if (client.stalkingVictims[i].match("^" + e.inputData +"$", "i"))
        {
            client.stalkingVictims.splice(i, 1);
            client.currentObject.display(getMsg("cli_iunstalkMsg", e.inputData),
                                         "UNSTALK");
            return true;
        }
    }

    client.currentObject.display(getMsg("cli_iunstalkMsg2", e.inputData),
                                 "UNSTALK");
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

CIRCNetwork.prototype.onInfo =
function my_netinfo (e)
{
    this.display (e.msg, "INFO");
}

CIRCNetwork.prototype.onUnknown =
function my_unknown (e)
{
    e.params.shift(); /* remove the code */
    e.params.shift(); /* and the dest. nick (always me) */
        /* if it looks like some kind of "end of foo" code, and we don't
         * already have a mapping for it, make one up */
    if (!(e.code in client.responseCodeMap) && e.meat.search (/^end of/i) != -1)
        client.responseCodeMap[e.code] = "---";
    
    this.display (e.params.join(" ") + ": " + e.meat,
                  e.code.toUpperCase());
}

CIRCNetwork.prototype.on001 = /* Welcome! */
CIRCNetwork.prototype.on002 = /* your host is */
CIRCNetwork.prototype.on003 = /* server born-on date */
CIRCNetwork.prototype.on004 = /* server id */
CIRCNetwork.prototype.on005 = /* server features */
CIRCNetwork.prototype.on250 = /* highest connection count */
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
    var p = (2 in e.params) ? e.params[2] + " " : "";
    var str = "";

    switch (e.code)
    {
        case "004":
        case "005":
            str = e.params.slice(3).join (" ");
            break;

        case "001":
            updateTitle(this);
            updateNetwork (this);
            if (client.currentObject == this)
            {
                var status = document.getElementById("offline-status");
                status.removeAttribute ("offline");
            }
            if ("pendingURLs" in this)
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

    this.displayHere (p + str, e.code.toUpperCase());
    
}

CIRCNetwork.prototype.onUnknownCTCPReply = 
function my_ctcprunk (e)
{
    this.display (getMsg("my_ctcprunk",
                         [e.CTCPCode, e.CTCPData, e.user.properNick]),
                         "CTCP_REPLY", e.user, e.server.me);
}

CIRCNetwork.prototype.onNotice = 
function my_notice (e)
{
    this.display (e.meat, "NOTICE", this, e.server.me);
}

CIRCNetwork.prototype.on303 = /* ISON (aka notify) reply */
function my_303 (e)
{
    var onList = stringTrim(e.meat.toLowerCase()).split(/\s+/);
    var offList = new Array();
    var newArrivals = new Array();
    var newDepartures = new Array();
    var o = getObjectDetails(client.currentObject);
    var displayTab;
    var i;

    if ("network" in o && o.network == this && client.currentObject != this)
        displayTab = client.currentObject;

    for (i in this.notifyList)
        if (!arrayContains(onList, this.notifyList[i]))
            /* user is not on */
            offList.push (this.notifyList[i]);
        
    if ("onList" in this)
    {
        for (i in onList)
            if (!arrayContains(this.onList, onList[i]))
                /* we didn't know this person was on */
                newArrivals.push(onList[i]);
    }
    else
        this.onList = newArrivals = onList;

    if ("offList" in this)
    {
        for (i in offList)
            if (!arrayContains(this.offList, offList[i]))
                /* we didn't know this person was off */
                newDepartures.push(offList[i]);
    }
    else
        this.offList = newDepartures = offList;
    
    if (newArrivals.length > 0)
    {
        this.displayHere (arraySpeak (newArrivals, "is", "are") +
                          " online.", "NOTIFY-ON");
        if (displayTab)
            displayTab.displayHere (arraySpeak (newArrivals, "is", "are") +
                                    " online.", "NOTIFY-ON");
    }
    
    if (newDepartures.length > 0)
    {
        this.displayHere (arraySpeak (newDepartures, "is", "are") +
                          " offline.", "NOTIFY-OFF");
        if (displayTab)
            displayTab.displayHere (arraySpeak (newDepartures, "is", "are") +
                                    " offline.", "NOTIFY-OFF");
    }

    this.onList = onList;
    this.offList = offList;
    
}

CIRCNetwork.prototype.on321 = /* LIST reply header */
function my_321 (e)
{

    function checkEndList (network)
    {
        if (network.list.count == network.list.lastLength)
        {
            network.on323();
        }
        else
        {
            network.list.lastLength = network.list.count;
            network.list.endTimeout =
                setTimeout(checkEndList, 1500, network);
        }
    }

    function outputList (network)
    {
        const CHUNK_SIZE = 5;
        var list = network.list;
        if (list.length > list.displayed)
        {
            var start = list.displayed;
            var end = list.length;
            if (end - start > CHUNK_SIZE)
                end = start + CHUNK_SIZE;
            for (var i = start; i < end; ++i)
                network.displayHere (getMsg("my_322", list[i]), "322");
            list.displayed = end;
        }
        if (list.done && (list.displayed == list.length))
        {
            if (list.event323)
            {
                network.displayHere (list.event323.params.join(" ") + ": " +
                                     list.event323.meat, "323");
            }
            network.displayHere (getMsg("my_323", [list.displayed, list.count]),
                                 "INFO");
            delete network.list;
        }
        else
        {
            setTimeout(outputList, 250, network);
        }
    }

    if (!("list" in this))
    {
        this.list = new Array();
        this.list.regexp = null;
    }
    this.displayHere (e.params[2] + " " + e.meat, "321");
    if (client.currentObject != this)
        client.currentObject.display (getMsg("my_321", this.name), "INFO");
    this.list.lastLength = 0;
    this.list.done = false;
    this.list.count = 0;
    this.list.displayed = 0;
    setTimeout(outputList, 250, this);
    this.list.endTimeout = setTimeout(checkEndList, 1500, this);
}

CIRCNetwork.prototype.on323 = /* end of LIST reply */
function my_323 (e)
{
    if (this.list.endTimeout)
    {
        clearTimeout(this.list.endTimeout);
        delete this.list.endTimeout;
    }
    this.list.done = true;
    this.list.event323 = e;
}

CIRCNetwork.prototype.on322 = /* LIST reply */
function my_listrply (e)
{
    ++this.list.count;
    e.params[2] = toUnicode(e.params[2]);
    if (!(this.list.regexp) || e.params[2].match(this.list.regexp)
                            || e.meat.match(this.list.regexp))
    {
        this.list.push([e.params[2], e.params[3], e.meat]);
    }
}

/* end of WHO */
CIRCNetwork.prototype.on315 =
function my_315 (e)
{
    var matches;
    if ("whoMatches" in this)
        matches = this.whoMatches;
    else
        matches = 0;
    e.user.display (getMsg("my_315", [e.params[2], matches]), e.code);
    delete this.whoMatches;
}

CIRCNetwork.prototype.on352 =
function my_352 (e)
{
    //0-352 1-rginda_ 2-#chatzilla 3-chatzilla 4-h-64-236-139-254.aoltw.net
    //5-irc.mozilla.org 6-rginda 7-H
    var desc;
    var hops = "?";
    var ary = e.meat.match(/(\d+)\s(.*)/);
    if (ary)
    {
        hops = Number(ary[1]);
        desc = ary[2];
    }
    else
    {
        desc = e.meat;
    }
    
    var status = e.params[7];
    if (e.params[7] == "G")
        status = getMsg("my_352.g");
    else if (e.params[7] == "H")
        status = getMsg("my_352.h");
        
    e.user.display (getMsg("my_352", [e.params[6], e.params[3], e.params[4],
                                      desc, status, toUnicode(e.params[2]), 
                                      e.params[5], hops]), e.code, e.user);
    updateTitle (e.user);
    if ("whoMatches" in this)
        ++this.whoMatches;
    else
        this.whoMatches = 1;
}

CIRCNetwork.prototype.on311 = /* whois name */
CIRCNetwork.prototype.on319 = /* whois channels */
CIRCNetwork.prototype.on312 = /* whois server */
CIRCNetwork.prototype.on317 = /* whois idle time */
CIRCNetwork.prototype.on318 = /* whois end of whois*/
function my_whoisreply (e)
{
    var text = "egads!";
    var nick = e.params[2];
    
    switch (Number(e.code))
    {
        case 311:
            text = getMsg("my_whoisreplyMsg",
                          [nick, e.params[3], e.params[4], e.meat]);
            break;
            
        case 319:
            var ary = stringTrim(e.meat).split(" ");
            text = getMsg("my_whoisreplyMsg2",[nick, arraySpeak(ary)]);
            break;
            
        case 312:
            text = getMsg("my_whoisreplyMsg3",
                          [nick, e.params[3], e.meat]);
            break;
            
        case 317:
            text = getMsg("my_whoisreplyMsg4",
                          [nick, formatDateOffset(Number(e.params[3])),
                          new Date(Number(e.params[4]) * 1000)]);
            break;
            
        case 318:
            text = getMsg("my_whoisreplyMsg5", nick);
            break;
            
    }

    if (nick in e.server.users && "messages" in e.server.users[nick])
    {
        var user = e.server.users[nick];
        updateTitle(user);
        user.display (text, e.code);
    }
    else
    {
        e.server.parent.display(text, e.code);
    }
}

CIRCNetwork.prototype.on341 = /* invite reply */
function my_341 (e)
{
    this.display (getMsg("my_341", [e.params[2], toUnicode(e.params[3])]), "341");
}

CIRCNetwork.prototype.onInvite = /* invite message */
function my_invite (e)
{
    this.display (getMsg("my_Invite", [e.user.properNick, e.user.name,
                                       e.user.host, e.meat]), "INVITE");
}

CIRCNetwork.prototype.on433 = /* nickname in use */
function my_433 (e)
{
    if (e.params[2] == CIRCNetwork.prototype.INITIAL_NICK && this.connecting)
    {
        var newnick = CIRCNetwork.prototype.INITIAL_NICK + "_";
        CIRCNetwork.prototype.INITIAL_NICK = newnick;
        e.server.parent.display (getMsg("my_433Retry", [e.params[2], newnick]),
                                 "433");
        this.primServ.sendData("NICK " + newnick + "\n");
    }
    else
    {
        this.display (getMsg("my_433Msg", e.params[2]), "433");
    }
}

CIRCNetwork.prototype.onStartConnect =
function my_sconnect (e)
{
    this.display (getMsg("my_sconnect", [this.name, e.host, e.port,
                                         e.connectAttempt,
                                         this.MAX_CONNECT_ATTEMPTS]), "INFO");
}
    
CIRCNetwork.prototype.onError =
function my_neterror (e)
{
    var msg;
    
    if (typeof e.errorCode != "undefined")
    {
        switch (e.errorCode)
        {
            case JSIRC_ERR_NO_SOCKET:
                msg = getMsg ("my_neterrorNoSocket");
                break;
                
            case JSIRC_ERR_EXHAUSTED:
                msg = getMsg ("my_neterrorExhausted");
                break;
        }
    }
    else
        msg = e.meat;
    
    this.display (msg, "ERROR");   
}


CIRCNetwork.prototype.onDisconnect =
function my_netdisconnect (e)
{
    var connection = e.server.connection;
    var msg;
    
    if (typeof e.disconnectStatus != "undefined")
    {
        switch (e.disconnectStatus)
        {
            case 0:
                msg = getMsg("my_netdisconnectConnectionClosed", [this.name,
                             connection.host, connection.port]);
                break;

            case NS_ERROR_CONNECTION_REFUSED:
                msg = getMsg("my_netdisconnectConnectionRefused", [this.name,
                             connection.host, connection.port]);
                break;

            case NS_ERROR_NET_TIMEOUT:
                msg = getMsg("my_netdisconnectConnectionTimeout", [this.name,
                             connection.host, connection.port]);
                break;

            case NS_ERROR_UNKNOWN_HOST:
                msg = getMsg("my_netdisconnectUnknownHost",
                             connection.host);
                break;
            
            default:
                msg = getMsg("my_netdisconnectConnectionClosedStatus",
                             [this.name, connection.host, connection.port]);
                break;
        }    
    }
    else
    {
        msg = getMsg("my_neterrorConnectionClosed", [this.name,
                     connection.host, connection.port]);
    }
    
    for (var v in client.viewsArray)
    {
        var obj = client.viewsArray[v].source;
        if (obj != client)
        {        
            var details = getObjectDetails(obj);
            if ("server" in details && details.server == e.server)
                obj.displayHere (msg, "ERROR");
        }
    }

    for (var c in this.primServ.channels)
    {
        var channel = this.primServ.channels[c];
        client.rdf.clearTargets(channel.getGraphResource(),
                                client.rdf.resChanUser);
    }
    
    this.connecting = false;
    updateTitle();
    if ("userClose" in client && client.userClose &&
        client.getConnectionCount() == 0)
        window.close();
}

CIRCNetwork.prototype.onCTCPReplyPing =
function my_replyping (e)
{
    var delay = formatDateOffset ((new Date() - new Date(Number(e.CTCPData))) /
                                  1000);
    display (getMsg("my_replyping", [e.user.properNick, delay]), "INFO",
             e.user, "ME!");
}

CIRCNetwork.prototype.onNick =
function my_cnick (e)
{

    if (userIsMe (e.user))
    {
        if (client.currentObject == this)
            this.displayHere (getMsg("my_cnickMsg", e.user.properNick),
                              "NICK", "ME!", e.user, this);
        updateNetwork();
    }
    else
        this.display (getMsg("my_cnickMsg2", [e.oldNick, e.user.properNick]),
                      "NICK", e.user, this);

}

CIRCNetwork.prototype.onPing =
function my_netping (e)
{

    updateNetwork (this);
    
}

CIRCNetwork.prototype.onPong =
function my_netpong (e)
{

    updateNetwork (this);
    
}

CIRCChannel.prototype.onPrivmsg =
function my_cprivmsg (e)
{
    
    this.display (e.meat, "PRIVMSG", e.user, this);
    
    if ((typeof client.prefix == "string") &&
        e.meat.indexOf (client.prefix) == 0)
    {
        try
        {
            var v = eval(e.meat.substring (client.prefix.length,
                                           e.meat.length));
        }
        catch (ex)
        {
            this.say (fromUnicode(e.user.nick + ": " + String(ex)));
            return false;
        }
        
        if (typeof (v) != "undefined")
        {						
            if (v != null)                
                v = String(v);
            else
                v = "null";
            
            var rsp = getMsg("my_cprivmsgMsg", e.user.nick);
            
            if (v.indexOf ("\n") != -1)
                rsp += "\n";
            else
                rsp += " ";
            
            this.display (rsp + v, "PRIVMSG", e.server.me, this);
            this.say (fromUnicode(rsp + v));
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
    
    if ("pendingNamesReply" in e.channel)
    {       
        display (e.meat, "366");
        e.channel.pendingNamesReply = false;
    }
}    

CIRCChannel.prototype.onTopic = /* user changed topic */
CIRCChannel.prototype.on332 = /* TOPIC reply */
function my_topic (e)
{

    if (e.code == "TOPIC")
        this.display (getMsg("my_topicMsg", [this.topicBy, this.topic]),
                      "TOPIC");
    
    if (e.code == "332")
    {
        if (this.topic)
            this.display (getMsg("my_topicMsg2", [this.unicodeName, this.topic]),
                          "TOPIC");
        else
            this.display (getMsg("my_topicMsg3", this.unicodeName), "TOPIC");
    }
    
    updateChannel (this);
    updateTitle (this);
    
}

CIRCChannel.prototype.on333 = /* Topic setter information */
function my_topicinfo (e)
{
    
    this.display (getMsg("my_topicinfoMsg", [this.unicodeName, this.topicBy, 
                                             this.topicDate]), "TOPIC");
    
}

CIRCChannel.prototype.on353 = /* names reply */
function my_topic (e)
{
    if ("pendingNamesReply" in e.channel)
        e.channel.display (e.meat, "NAMES");
}


CIRCChannel.prototype.onNotice =
function my_notice (e)
{
    this.display (e.meat, "NOTICE", e.user, this);   
}

CIRCChannel.prototype.onCTCPAction =
function my_caction (e)
{

    this.display (e.CTCPData, "ACTION", e.user, this);

}

CIRCChannel.prototype.onUnknownCTCP =
function my_unkctcp (e)
{

    this.display (getMsg("my_unkctcpMsg", [e.CTCPCode, e.CTCPData,
                                           e.user.properNick]),
                  "BAD-CTCP", e.user, this);
    
}   

CIRCChannel.prototype.onJoin =
function my_cjoin (e)
{

    if (userIsMe (e.user))
    {
        this.display (getMsg("my_cjoinMsg", e.channel.unicodeName), "JOIN",
                      e.server.me, this);
        setCurrentObject(this);
    }
    else
        this.display(getMsg("my_cjoinmsg2", [e.user.properNick, e.user.name,
                                             e.user.host, e.channel.unicodeName]),
                     "JOIN", e.user, this);

    this._addUserToGraph (e.user);
    
    updateChannel (e.channel);
    
}

CIRCChannel.prototype.onPart =
function my_cpart (e)
{

    this._removeUserFromGraph(e.user);

    if (userIsMe (e.user))
    {
        this.display (getMsg("my_cpartMsg", e.channel.unicodeName), "PART",
                      e.user, this);
        if (client.currentObject == this)    
            /* hide the tree while we remove (possibly tons) of nodes */
            client.rdf.setTreeRoot("user-list", client.rdf.resNullChan);
        
        client.rdf.clearTargets(this.getGraphResource(),
                                client.rdf.resChanUser, true);

        if (client.currentObject == this)
            /* redisplay the tree */
            client.rdf.setTreeRoot("user-list", this.getGraphResource());

        if (client.DELETE_ON_PART)
            onDeleteView(this);
    }
    else
        this.display (getMsg("my_cpartMsg2",
                             [e.user.properNick, e.channel.unicodeName]), 
                      "PART", e.user, this);

    updateChannel (e.channel);
    
}

CIRCChannel.prototype.onKick =
function my_ckick (e)
{

    if (userIsMe (e.lamer))
        this.display (getMsg("my_ckickMsg",
                             [e.channel.unicodeName, e.user.properNick, e.reason]),
                      "KICK", e.user, this);
    else
    {
        var enforcerProper, enforcerNick;
        if (userIsMe (e.user))
        {
            enforcerProper = "YOU";
            enforcerNick = "ME!";
        }
        else
        {
            enforcerProper = e.user.properNick;
            enforcerNick = e.user.nick;
        }
        
        this.display (getMsg("my_ckickMsg2",
                             [e.lamer.properNick, e.channel.unicodeName, 
                              enforcerProper, e.reason]), "KICK", e.user, this);
    }
    
    this._removeUserFromGraph(e.lamer);

    updateChannel (e.channel);
    
}

CIRCChannel.prototype.onChanMode =
function my_cmode (e)
{

    if ("user" in e)
        this.display (getMsg("my_cmodeMsg",
                             [toUnicode(e.params.slice(1).join(" ")),
                              e.user.properNick]), "MODE", e.user, this);

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
        this.display (getMsg("my_cnickMsg", e.user.properNick), "NICK",
                      "ME!", e.user, this);
        updateNetwork();
    }
    else
        this.display (getMsg("my_cnickMsg2", e.oldNick, e.user.properNick),
                      "NICK", e.user, this);

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
        this.display (getMsg("my_cquitMsg", [e.server.parent.name, e.reason]),
                      "QUIT", e.user, this);
    else
        this.display (getMsg("my_cquitMsg2", [e.user.properNick,
                                              e.server.parent.name, e.reason]),
                      "QUIT", e.user, this);

    this._removeUserFromGraph(e.user);

    updateChannel (e.channel);
    
}

CIRCUser.prototype.onPrivmsg =
function my_cprivmsg (e)
{
    if ("messages" in this)
    {
        playSounds(client.QUERY_BEEP);
    }
    else
    {        
        playSounds(client.MSG_BEEP);
        if (client.NEW_TAB_LIMIT == 0 ||
            client.viewsArray.length < client.NEW_TAB_LIMIT)
        {
            var tab = openQueryTab (e.server, e.user.nick);
            if (client.FOCUS_NEW_TAB)
                setCurrentObject(tab);
        }    
    }
    this.display (e.meat, "PRIVMSG", e.user, e.server.me);
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
    this.display (e.meat, "NOTICE", this, e.server.me);   
}

CIRCUser.prototype.onCTCPAction =
function my_uaction (e)
{

    e.user.display (e.CTCPData, "ACTION", this, e.server.me);

}

CIRCUser.prototype.onUnknownCTCP =
function my_unkctcp (e)
{

    this.parent.parent.display (getMsg("my_unkctcpMsg",
                                       [e.CTCPCode, e.CTCPData,
                                       e.user.properNick]),
                                "BAD-CTCP", this, e.server.me);

}

