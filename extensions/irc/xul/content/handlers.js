/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is ChatZilla.
 *
 * The Initial Developer of the Original Code is
 * New Dimensions Consulting, Inc.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Robert Ginda, rginda@netscape.com, original author
 *   Samuel Sieb, samuel@sieb.net
 *   Chiaki Koufugata chiaki@mozilla.gr.jp UI i18n
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

window.onresize =
function onresize()
{
    for (var i = 0; i < client.deck.childNodes.length; i++)
        scrollDown(client.deck.childNodes[i], true);
}

function onInputFocus()
{
}

function onLoad()
{
    dd ("Initializing ChatZilla {");
    try
    {
        init();
    }
    catch (ex)
    {
        dd("caught exception while initializing:\n" + dumpObjectTree(ex));
        
        setTimeout(function delayError() {
                       baseAlert("The was an error starting ChatZilla. " +
                                 "Please report the following information:\n" + 
                                 dumpObjectTree(ex));
                   }, 100);
    }
    
    dd("}");
    mainStep();
}

function initHandlers()
{
    var node;
    node = document.getElementById("input");
    node.addEventListener("keypress", onInputKeyPress, false);
    node = document.getElementById("multiline-input");
    node.addEventListener("keypress", onMultilineInputKeyPress, false);
    node.active = false;

    window.onkeypress = onWindowKeyPress;
    
    window.isFocused = false;
    window.addEventListener("focus", onWindowFocus, true);
    window.addEventListener("blur", onWindowBlue, true);
    
    client.inputPopup = null;
}

function onClose()
{
    if ("userClose" in client && client.userClose)
        return true;
    
    client.userClose = true;
    display(MSG_CLOSING);

    if (!("getConnectionCount" in client) ||
        client.getConnectionCount() == 0)
    {
        /* if we're not connected to anything, just close the window */
        return true;
    }

    /* otherwise, try to close out gracefully */
    client.quit(client.userAgent);
    return false;
}

function onUnload()
{
    dd("Shutting down ChatZilla.");
    destroy();
}

function onNotImplemented()
{
    alert (getMsg("onNotImplementedMsg"));
}

/* tab click */
function onTabClick (e, id)
{
    if (e.which != 1)
        return;
    
    var tbi = document.getElementById (id);
    var view = client.viewsArray[tbi.getAttribute("viewKey")];

    setCurrentObject (view.source);
}

function onMessageViewClick(e)
{
    if ((e.which != 1) && (e.which != 2))
        return true;
    
    var cx = getMessagesContext(null, e.target);
    var command;
    
    if (e.which == 2)
        command = client.prefs["messages.middleClick"];
    else if (e.metaKey || e.altKey)
        command = client.prefs["messages.metaClick"];
    else if (e.ctrlKey)
        command = client.prefs["messages.ctrlClick"];
    else
        command = client.prefs["messages.click"];
    
    if (client.commandManager.isCommandSatisfied(cx, command))
    {
        dispatch(command, cx);
        e.preventDefault();
        return true;
    }

    return false;
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
    
function onSortCol(sortColName)
{
    var node = document.getElementById(sortColName);
    if (!node)
        return false;
 
    // determine column resource to sort on
    var sortResource = node.getAttribute("resource");
    var sortDirection = node.getAttribute("sortDirection");
    
    if (sortDirection == "ascending")
        sortDirection = "descending";
    else
        sortDirection = "ascending";
    
    sortUserList(node, sortDirection);
    
    return false;
}

function onMultilineInputKeyPress (e)
{
    if ((e.ctrlKey || e.metaKey) && e.keyCode == 13)
    {
        /* meta-enter, execute buffer */
        onMultilineSend(e);
    }
    else
    {
        if ((e.ctrlKey || e.metaKey) && e.keyCode == 40)
        {
            /* ctrl/meta-down, switch to single line mode */
            dispatch ("pref multiline false");
        }
    }
}

function onMultilineSend(e)
{
    var multiline = document.getElementById("multiline-input");
    e.line = multiline.value;
    if (e.line.search(/\S/) == -1)
        return;
    onInputCompleteLine (e);
    multiline.value = "";
}

function onTooltip(event)
{
    const XLinkNS = "http://www.w3.org/1999/xlink";

    var tipNode = event.originalTarget;
    var titleText = null;
    var XLinkTitleText = null;
    
    var element = document.tooltipNode;
    while (element)
    {
        if (element.nodeType == Node.ELEMENT_NODE)
        {
            var text;
            if (element.hasAttribute("title"))
                text = element.getAttribute("title");
            else if (element.hasAttributeNS(XLinkNS, "title"))
                text = element.getAttributeNS(XLinkNS, "title");

            if (text)
            {
                tipNode.setAttribute("label", text);
                return true;
            }
        }

        element = element.parentNode;
    }

    return false;
}
    
function onInputKeyPress (e)
{
    if (client.prefs["outgoing.colorCodes"])
        setTimeout(onInputKeyPressCallback, 100, e.target);
    
    switch (e.keyCode)
    {        
        case 9:  /* tab */
            if (!e.ctrlKey && !e.metaKey)
            {
                onTabCompleteRequest(e);
                e.preventDefault();
            }
            break;

        case 13: /* CR */
            e.line = e.target.value;
            e.target.value = "";
            if (e.line.search(/\S/) == -1)
                return;
            onInputCompleteLine (e);
            break;

        case 38: /* up */
            if (e.ctrlKey || e.metaKey)
            {
                /* ctrl/meta-up, switch to multi line mode */
                dispatch ("pref multiline true");
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
            e.preventDefault();
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
            e.preventDefault();
            break;

        default:
            client.lastHistoryReferenced = -1;
            client.incompleteLine = e.target.value;
            break;
    }

}

function onTabCompleteRequest (e)
{
    var elem = document.commandDispatcher.focusedElement;
    var singleInput = document.getElementById("input");
    if (document.getBindingParent(elem) != singleInput)
        return;

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
        var wordLower = word.toLowerCase();
        var d = getObjectDetails(client.currentObject);
        if (d.server)
            wordLower = d.server.toLowerCase(word);
        var matches = client.currentObject.performTabMatch (line, wordStart,
                                                            wordEnd,
                                                            wordLower,
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
                display(getMsg(MSG_FMT_MATCHLIST,
                               [matches.length, word,
                                matches.join(MSG_COMMASP)]));
            }
            else
            {
                /* or display an error if there are none. */
                display(getMsg(MSG_ERR_NO_MATCH, word), MT_ERROR);
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
    var code = Number(e.keyCode);
    var w;
    var newOfs;
    var userList = document.getElementById("user-list");
    var elemFocused = document.commandDispatcher.focusedElement;

    switch (code)
    {
        case 9: /* tab */
            if (e.ctrlKey || e.metaKey)
            {
                cycleView(e.shiftKey ? -1: 1);
                e.preventDefault();
            }
            break;

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
            if ((idx in client.viewsArray) && client.viewsArray[idx].source)
                setCurrentObject(client.viewsArray[idx].source);
            break;

        case 33: /* pgup */
            if (e.ctrlKey)
            {
                cycleView(-1);
                break;
            }

            if (elemFocused == userList)
                break;

            w = client.currentFrame;
            newOfs = w.pageYOffset - (w.innerHeight * 0.75);
            if (newOfs > 0)
                w.scrollTo (w.pageXOffset, newOfs);
            else
                w.scrollTo (w.pageXOffset, 0);
            e.preventDefault();
            break;
            
        case 34: /* pgdn */
            if (e.ctrlKey)
            {
                cycleView(1);
                break;
            }

            if (elemFocused == userList)
                break;

            w = client.currentFrame;
            newOfs = w.pageYOffset + (w.innerHeight * 0.75);
            if (newOfs < (w.innerHeight + w.pageYOffset))
                w.scrollTo (w.pageXOffset, newOfs);
            else
                w.scrollTo (w.pageXOffset, (w.innerHeight + w.pageYOffset));
            e.preventDefault();
            break;
    }
}

function onWindowFocus(e)
{
    window.isFocused = true;
}

function onWindowBlue(e)
{
    window.isFocused = false;
}

function onInputCompleteLine(e)
{
    if (!client.inputHistory.length || client.inputHistory[0] != e.line)
        client.inputHistory.unshift (e.line);
    
    if (client.inputHistory.length > client.MAX_HISTORY)
        client.inputHistory.pop();
    
    client.lastHistoryReferenced = -1;
    client.incompleteLine = "";
    
    if (e.line[0] == client.COMMAND_CHAR)
    {
        if (client.prefs["outgoing.colorCodes"])
            e.line = replaceColorCodes(e.line);
        dispatch(e.line.substr(1), null, true);
    }
    else /* plain text */
    {
        /* color codes */
        if (client.prefs["outgoing.colorCodes"])
            e.line = replaceColorCodes(e.line);
        client.sayToCurrentTarget (e.line);
    }
}

function onNotifyTimeout ()
{
    for (var n in client.networks)
    {
        var net = client.networks[n];
        if (net.isConnected()) {
            if (net.prefs["notifyList"].length > 0) {
                var isonList = client.networks[n].prefs["notifyList"];
                net.primServ.sendData ("ISON " + isonList.join(" ") + "\n");
            } else {
                /* if the notify list is empty, just send a ping to see if we're
                 * alive. */
                net.primServ.sendData ("PING :ALIVECHECK\n");
            }
        }
    }
}

function onInputKeyPressCallback (el)
{
    function doPopup(popup)
    {
        if (client.inputPopup && client.inputPopup != popup)
            client.inputPopup.hidePopup();
        
        client.inputPopup = popup;
        if (popup)
        {
            var box = el.boxObject;
            var pos;
            if (!box)
                box = el.ownerDocument.getBoxObjectFor(el);
            if (el.nodeName == "textbox")
                pos = { x: box.screenX, 
                        y: box.screenY - box.height - popup.boxObject.height };
            else
                pos = { x: box.screenX + 5, 
                        y: box.screenY + box.height + 25 };
            popup.moveTo(pos.x, pos.y);
            popup.showPopup(el, 0, 0, "tooltip");
        }
    }
    
    var text = " " + el.value.substr(0, el.selectionStart);
    if (el.selectionStart != el.selectionEnd)
        text = "";
    
    if (text.match(/[^%]%C[0-9]{0,2},?[0-9]{0,2}$/))
        doPopup(document.getElementById("colorTooltip"));
    else if (text.match(/[^%]%$/))
        doPopup(document.getElementById("percentTooltip"));
    else
        doPopup(null);
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

CIRCNetwork.prototype.onInit =
function net_oninit ()
{
    this.logFile = null;
    this.lastServer = null;
}

CIRCNetwork.prototype.onInfo =
function my_netinfo (e)
{
    this.display (e.msg, "INFO");
}

CIRCNetwork.prototype.onUnknown =
function my_unknown (e)
{
    if ("pendingWhoisLines" in e.server)
    {
        /* whois lines always have the nick in param 2 */
        e.user = new CIRCUser(e.server, e.params[2]);
        
        e.destMethod = "onUnknownWhois";
        e.destObject = this;
        return;
    }
    
    e.params.shift(); /* remove the code */
    e.params.shift(); /* and the dest. nick (always me) */
    
    // Handle random IRC numerics automatically.
    var msg = getMsg("msg.err.irc." + e.code, null, "");
    if (msg)
    {
        this.display(msg);
        return;
    }
    
        /* if it looks like some kind of "end of foo" code, and we don't
         * already have a mapping for it, make one up */
    var length = e.params.length;
    if (!(e.code in client.responseCodeMap) && 
        (e.params[length - 1].search (/^end of/i) != -1))
    {
        client.responseCodeMap[e.code] = "---";
    }
    
    this.display(toUnicode(e.params.join(" "), this), e.code.toUpperCase());
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
    var p = (3 in e.params) ? e.params[2] + " " : "";
    var str = "";

    switch (e.code)
    {
        case "004":
        case "005":
            str = e.params.slice(3).join(" ");
            break;

        case "001":
            // Code moved to lower down to speed this bit up. :)
            
            var cmdary = this.prefs["autoperform"];
            for (var i = 0; i < cmdary.length; ++i)
                this.dispatch(cmdary[i])
            
            if (this.lastServer)
            {
                var c, u;
                
                // Re-home channels and users only if nessessary.
                if (this.lastServer != this.primServ)
                {
                    for (c in this.lastServer.channels)
                        this.lastServer.channels[c].rehome(this.primServ);
                    for (u in this.lastServer.users)
                        this.lastServer.users[u].rehome(this.primServ);
                    
                    // This makes sure we have the *right* me object.
                    this.primServ.me.rehome(this.primServ);
                }
                
                // Re-join channels from previous connection.
                // (note they're all on .primServ now, not .lastServer)
                for (c in this.primServ.channels)
                {
                    var chan = this.primServ.channels[c];
                    if (chan.joined)
                        chan.join(chan.mode.key);
                }
            }
            this.lastServer = this.primServ;
            
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
            
            // Update everything.
            // Welcome to history.
            client.globalHistory.addPage(this.getURL());
            updateTitle(this);
            this.updateHeader();
            client.updateHeader();
            updateStalkExpression(this);
            
            // Do this after the JOINs, so they are quicker.
            // This is not time-critical code.
            if (jsenv.HAS_SERVER_SOCKETS && client.prefs["dcc.enabled"] && 
                this.prefs["dcc.useServerIP"])
            {
                // This is the quickest way to get out host/IP.
                this.pendingUserhostReply = true;
                this.primServ.sendData("USERHOST " + 
                                       this.primServ.me.nick + "\n");
            }
            
            str = e.decodeParam(2);
            if ("onLogin" in this)
            {
                ev = new CEvent("network", "login", this, "onLogin");
                client.eventPump.addEvent(ev);
            }
            break;
            
        case "372":
        case "375":
        case "376":
            if (this.IGNORE_MOTD)
                return;
            /* no break */

        default:
            str = e.decodeParam(e.params.length - 1);
            break;
    }

    this.displayHere(p + str, e.code.toUpperCase());
    
}

CIRCNetwork.prototype.onUnknownCTCPReply = 
function my_ctcprunk (e)
{
    this.display(getMsg(MSG_FMT_CTCPREPLY,
                        [toUnicode(e.CTCPCode, this), 
                         toUnicode(e.CTCPData, this), e.user.properNick]),
                 "CTCP_REPLY", e.user, e.server.me, this);
}

CIRCNetwork.prototype.onNotice = 
function my_notice (e)
{
    this.display(e.decodeParam(2), "NOTICE", this, e.server.me);
}

/* userhost reply */
CIRCNetwork.prototype.on302 = 
function my_302(e)
{
    if (jsenv.HAS_SERVER_SOCKETS && client.prefs["dcc.enabled"] && 
        this.prefs["dcc.useServerIP"] && ("pendingUserhostReply" in this))
    {
        var me = new RegExp("^" + this.primServ.me.nick + "\\*?=", "i");
        if (e.params[2].match(me))
            client.dcc.addHost(this.primServ.me.host, true);
        
        delete this.pendingUserhostReply;
        return true;
    }
    
    e.destMethod = "onUnknown";
    e.destObject = this;
    
    return true;
}

CIRCNetwork.prototype.on303 = /* ISON (aka notify) reply */
function my_303 (e)
{
    var onList = new Array();
    // split() gives an array of one item ("") when splitting "", which we
    // don't want, so only do the split if there's something to split.
    if (e.params[2])
        onList = stringTrim(e.server.toLowerCase(e.params[2])).split(/\s+/);
    var offList = new Array();
    var newArrivals = new Array();
    var newDepartures = new Array();
    var o = getObjectDetails(client.currentObject);
    var displayTab;
    var i;

    if ("network" in o && o.network == this && client.currentObject != this)
        displayTab = client.currentObject;

    for (i = 0; i < this.prefs["notifyList"].length; i++)
    {
        if (!arrayContains(onList, this.prefs["notifyList"][i]))
            /* user is not on */
            offList.push (this.prefs["notifyList"][i]);
    }
        
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

CIRCNetwork.prototype.listInit =
function my_list_init ()
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
                network.displayHere (getMsg(MSG_FMT_CHANLIST, list[i]), "322");
            list.displayed = end;
        }
        if (list.done && (list.displayed == list.length))
        {
            if (list.event323)
            {
                var length = list.event323.params.length;
                network.displayHere (list.event323.params[length - 1], "323");
            }
            network.displayHere(getMsg(MSG_LIST_END,
                                       [list.displayed, list.count]));
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
    if (client.currentObject != this)
        display (getMsg(MSG_LIST_REROUTED, this.name));
    this.list.lastLength = 0;
    this.list.done = false;
    this.list.count = 0;
    this.list.displayed = 0;
    setTimeout(outputList, 250, this);
    this.list.endTimeout = setTimeout(checkEndList, 1500, this);
}

CIRCNetwork.prototype.on321 = /* LIST reply header */
function my_321 (e)
{

    this.listInit();
    this.displayHere (e.params[2] + " " + e.params[3], "321");
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
    if (!("list" in this) || !("done" in this.list))
        this.listInit();
    ++this.list.count;
    
    var chanName = e.decodeParam(2);
    var topic = e.decodeParam(4);
    if (!this.list.regexp || chanName.match(this.list.regexp) ||
        topic.match(this.list.regexp))
    {
        this.list.push([chanName, e.params[3], topic]);
    }
}

CIRCNetwork.prototype.on401 =
function my_401 (e)
{
    var target = e.server.toLowerCase(e.params[2]);
    if (target in this.users && "messages" in this.users[target])
    {
        this.users[target].displayHere(e.params[3]);
    }
    else if (target in this.primServ.channels &&
             "messages" in this.primServ.channels[target])
    {
        this.primServ.channels[target].displayHere(e.params[3]);
    }
    else
    {
        display(toUnicode(e.params[3], this));
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
    e.user.display (getMsg(MSG_WHO_END, [e.params[2], matches]), e.code);
    e.user.updateHeader();
    delete this.whoMatches;
}

CIRCNetwork.prototype.on352 =
function my_352 (e)
{
    //0-352 1-rginda_ 2-#chatzilla 3-chatzilla 4-h-64-236-139-254.aoltw.net
    //5-irc.mozilla.org 6-rginda 7-H
    var desc;
    var hops = "?";
    var length = e.params.length;
    var ary = e.params[length - 1].match(/(\d+)\s(.*)/);
    if (ary)
    {
        hops = Number(ary[1]);
        desc = toUnicode(ary[2], this);
    }
    else
    {
        desc = e.decodeParam(length - 1);
    }
    
    var status = e.params[7];
    if (e.params[7][0] == "G")
        status = MSG_GONE;
    else if (e.params[7][0] == "H")
        status = MSG_HERE;
        
    e.user.display(getMsg(MSG_WHO_MATCH,
                           [e.params[6], e.params[3], e.params[4],
                            desc, status,
                            e.decodeParam(2), 
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
CIRCNetwork.prototype.onUnknownWhois = /* misc whois line */
function my_whoisreply (e)
{
    var text = "egads!";
    var nick = e.params[2];
    var user;
    
    if (e.user)
        user = e.user;
    
    dd("WHOIS: " + nick + ", " + e.user);
    
    switch (Number(e.code))
    {
        case 311:
            text = getMsg(MSG_WHOIS_NAME,
                          [nick, e.params[3], e.params[4],
                           e.decodeParam(6)]);
            break;
            
        case 319:
            var ary = stringTrim(e.decodeParam(3)).split(" ");
            text = getMsg(MSG_WHOIS_CHANNELS, [nick, arraySpeak(ary)]);
            break;
            
        case 312:
            text = getMsg(MSG_WHOIS_SERVER,
                          [nick, e.params[3], e.params[4]]);
            break;
            
        case 317:
            text = getMsg(MSG_WHOIS_IDLE,
                          [nick, formatDateOffset(Number(e.params[3])),
                          new Date(Number(e.params[4]) * 1000)]);
            break;
            
        case 318:
            text = getMsg(MSG_WHOIS_END, nick);
            if (user)
                user.updateHeader();
            break;
            
        default:
            text = toUnicode(e.params.splice(2, e.params.length).join(" "), 
                             this);
    }
    
    //if (user && "messages" in user)
    //    user.displayHere(text, e.code);
    //else
    //    e.server.parent.display(text, e.code);
    this.display(text, e.code);
}

CIRCNetwork.prototype.on330 = /* ircu's 330 numeric ("X is logged in as Y") */
function my_330 (e)
{
    this.display (getMsg(MSG_FMT_LOGGED_ON, [e.params[2], e.params[3]]),
                  "330");
}

CIRCNetwork.prototype.on341 = /* invite reply */
function my_341 (e)
{
    this.display (getMsg(MSG_YOU_INVITE, [e.params[2], e.decodeParam(3)]),
                  "341");
}

CIRCNetwork.prototype.onInvite = /* invite message */
function my_invite (e)
{
    this.display (getMsg(MSG_INVITE_YOU, [e.user.properNick, e.user.name,
                                          e.user.host, e.params[2]]), "INVITE");
}

CIRCNetwork.prototype.on433 = /* nickname in use */
function my_433 (e)
{
    if (e.params[2] == this.INITIAL_NICK && this.connecting)
    {
        var newnick = this.INITIAL_NICK + "_";
        this.INITIAL_NICK = newnick;
        e.server.parent.display (getMsg(MSG_RETRY_NICK, [e.params[2], newnick]),
                                 "433");
        this.primServ.sendData("NICK " + newnick + "\n");
    }
    else
    {
        this.display (getMsg(MSG_NICK_IN_USE, e.params[2]), "433");
    }
}

CIRCNetwork.prototype.onStartConnect =
function my_sconnect (e)
{
    this.display (getMsg(MSG_CONNECTION_ATTEMPT,
                         [this.name, e.host, e.port, e.connectAttempt,
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
                msg = MSG_ERR_NO_SOCKET;
                break;
                
            case JSIRC_ERR_EXHAUSTED:
                msg = MSG_ERR_EXHAUSTED;
                break;
        }
    }
    else
        msg = e.params[e.params.length - 1];
    
    this.display (msg, "ERROR");   
}


CIRCNetwork.prototype.onDisconnect =
function my_netdisconnect (e)
{
    var msg;
    var reconnect = false;
    
    if (typeof e.disconnectStatus != "undefined")
    {
        switch (e.disconnectStatus)
        {
            case 0:
                msg = getMsg(MSG_CONNECTION_CLOSED,
                             [this.getURL(), e.server.getURL()]);
                break;

            case NS_ERROR_CONNECTION_REFUSED:
                msg = getMsg(MSG_CONNECTION_REFUSED,
                             [this.getURL(), e.server.getURL()]);
                break;

            case NS_ERROR_NET_TIMEOUT:
                msg = getMsg(MSG_CONNECTION_TIMEOUT,
                             [this.getURL(), e.server.getURL()]);
                break;

            case NS_ERROR_NET_RESET:
                msg = getMsg(MSG_CONNECTION_RESET,
                             [this.getURL(), e.server.getURL()]);
                break;
            
            case NS_ERROR_UNKNOWN_HOST:
                msg = getMsg(MSG_UNKNOWN_HOST,
                             e.server.hostname);
                break;
            
            default:
                msg = getMsg(MSG_CLOSE_STATUS,
                             [this.getURL(), e.server.getURL(), 
                              e.disconnectStatus]);
                reconnect = true;
                break;
        }    
    }
    else
    {
        msg = getMsg(MSG_CONNECTION_CLOSED,
                     [this.name, e.server.hostname, e.server.port]);
    }
    
    /* If we were only /trying/ to connect, and failed, just put an error on
     * the network tab. If we were actually connected ok, put it on all tabs.
     */
    if (this.connecting)
    {
        this.displayHere(msg, "ERROR");
    }
    else
    {
        for (var v in client.viewsArray)
        {
            var obj = client.viewsArray[v].source;
            if (obj != client)
            {        
                var details = getObjectDetails(obj);
                if ("server" in details && details.server == e.server)
                    obj.displayHere(msg, "ERROR");
            }
        }
    }

    for (var c in this.primServ.channels)
    {
        var channel = this.primServ.channels[c];
        client.rdf.clearTargets(channel.getGraphResource(),
                                client.rdf.resChanUser);
    }
    
    this.connecting = false;
    dispatch("sync-headers");
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
    display (getMsg(MSG_PING_REPLY, [e.user.properNick, delay]), "INFO",
             e.user, "ME!");
}

CIRCNetwork.prototype.on221 =
CIRCNetwork.prototype.onUserMode =
function my_umode (e)
{
    if ("user" in e && e.user)
        e.user.updateHeader();

    display(getMsg(MSG_USER_MODE, [e.params[1], e.params[2]]), MT_MODE);
}

CIRCNetwork.prototype.onNick =
function my_cnick (e)
{
    if (!ASSERT(userIsMe(e.user), "network nick event for third party"))
        return;

    if (getTabForObject(this))
    {
        this.displayHere(getMsg(MSG_NEWNICK_YOU, e.user.properNick),
                         "NICK", "ME!", e.user, this);
    }
    
    this.updateHeader();
    updateStalkExpression(this);
}

CIRCNetwork.prototype.onPing =
function my_netping (e)
{
    this.updateHeader(this);
}

CIRCNetwork.prototype.onPong =
function my_netpong (e)
{
    this.updateHeader(this);
}

CIRCChannel.prototype.onInit =
function chan_oninit ()
{
    this.logFile = null;
    this.pendingNamesReply = false;
}

CIRCChannel.prototype.onPrivmsg =
function my_cprivmsg (e)
{
    var msg = e.decodeParam(2);
    
    this.display (msg, "PRIVMSG", e.user, this);
    
    if ((typeof client.prefix == "string") &&
        msg.indexOf (client.prefix) == 0)
    {
        try
        {
            var v = eval(msg.substring(client.prefix.length, msg.length));
        }
        catch (ex)
        {
            this.say(e.user.nick + ": " + String(ex));
            return false;
        }
        
        if (typeof v != "undefined")
        {
            if (v != null)
                v = String(v);
            else
                v = "null";
            
            var rsp = getMsg(MSG_PREFIX_RESPONSE, e.user.nick);
            
            if (v.indexOf ("\n") != -1)
                rsp += "\n";
            else
                rsp += " ";
            
            this.display(rsp + v, "PRIVMSG", e.server.me, this);
            this.say(rsp + v, this);
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
    
    if (this.pendingNamesReply)
    {
        this.parent.parent.display (e.channel.unicodeName + ": " + 
                                    e.params[3], "366");
    }
    this.pendingNamesReply = false;
}    

CIRCChannel.prototype.onTopic = /* user changed topic */
CIRCChannel.prototype.on332 = /* TOPIC reply */
function my_topic (e)
{

    if (e.code == "TOPIC")
        this.display (getMsg(MSG_TOPIC_CHANGED, [this.topicBy, this.topic]),
                      "TOPIC");
    
    if (e.code == "332")
    {
        if (this.topic)
        {
            this.display (getMsg(MSG_TOPIC,
                                 [this.unicodeName, this.topic]),
                          "TOPIC");
        }
        else
        {
            this.display (getMsg(MSG_NO_TOPIC, this.unicodeName), "TOPIC");
        }
    }
    
    this.updateHeader();
    updateTitle(this);
    
}

CIRCChannel.prototype.on333 = /* Topic setter information */
function my_topicinfo (e)
{
    this.display (getMsg(MSG_TOPIC_DATE, [this.unicodeName, this.topicBy, 
                                          this.topicDate]), "TOPIC");
}

CIRCChannel.prototype.on353 = /* names reply */
function my_topic (e)
{
    if (this.pendingNamesReply)
    {
        this.parent.parent.display (e.channel.unicodeName + ": " + 
                                    e.params[4], "NAMES");
    }
}

CIRCChannel.prototype.on367 = /* channel ban stuff */
CIRCChannel.prototype.on368 =
function my_bans(e)
{
    // Uh, we'll format this some other time.
    if (!("pendingBanList" in this))
        this.display(toUnicode(e.params.join(" "), this), e.code.toUpperCase());
}

CIRCChannel.prototype.on348 = /* channel except stuff */
CIRCChannel.prototype.on349 =
function my_excepts(e)
{
    if (!("pendingExceptList" in this))
        this.display(toUnicode(e.params.join(" "), this), e.code.toUpperCase());
}


CIRCChannel.prototype.onNotice =
function my_notice (e)
{
    this.display(e.decodeParam(2), "NOTICE", e.user, this);   
}

CIRCChannel.prototype.onCTCPAction =
function my_caction (e)
{
    this.display (e.CTCPData, "ACTION", e.user, this);
}

CIRCChannel.prototype.onUnknownCTCP =
function my_unkctcp (e)
{
    this.display (getMsg(MSG_UNKNOWN_CTCP, [e.CTCPCode, e.CTCPData, 
                                            e.user.properNick]),
                  "BAD-CTCP", e.user, this);
}   

CIRCChannel.prototype.onJoin =
function my_cjoin (e)
{
    if (!("messages" in this))
        this.displayHere(getMsg(MSG_CHANNEL_OPENED, this.unicodeName), MT_INFO);
    
    if (userIsMe (e.user))
    {
        this.display (getMsg(MSG_YOU_JOINED, e.channel.unicodeName), "JOIN",
                      e.server.me, this);
        client.globalHistory.addPage(this.getURL());
        
        /* !-channels are "safe" channels, and get a server-generated prefix.
         * For this reason, creating the channel is delayed until this point.
         */
        if (e.channel.unicodeName[0] == "!")
            setCurrentObject(e.channel);
    }
    else
    {
        this.display(getMsg(MSG_SOMEONE_JOINED,
                            [e.user.properNick, e.user.name, e.user.host,
                             e.channel.unicodeName]),
                     "JOIN", e.user, this);
    }

    this._addUserToGraph (e.user);
    updateUserList()
    this.updateHeader();
}

CIRCChannel.prototype.onPart =
function my_cpart (e)
{
    this._removeUserFromGraph(e.user);

    if (userIsMe (e.user))
    {
        this.display (getMsg(MSG_YOU_LEFT, e.channel.unicodeName), "PART",
                      e.user, this);
        if (client.currentObject == this)    
            /* hide the tree while we remove (possibly tons) of nodes */
            client.rdf.setTreeRoot("user-list", client.rdf.resNullChan);
        
        client.rdf.clearTargets(this.getGraphResource(),
                                client.rdf.resChanUser, true);

        if (client.currentObject == this)
            /* redisplay the tree */
            client.rdf.setTreeRoot("user-list", this.getGraphResource());

        if ("noDelete" in this)
            delete this.noDelete;
        else if (client.prefs["deleteOnPart"])
            this.dispatch("delete");
    }
    else
    {
        if (e.reason)
        {
            this.display (getMsg(MSG_SOMEONE_LEFT_REASON,
                                 [e.user.properNick, e.channel.unicodeName,
                                  e.reason]),
                          "PART", e.user, this);
        }
        else
        {
            this.display (getMsg(MSG_SOMEONE_LEFT,
                                 [e.user.properNick, e.channel.unicodeName]),
                          "PART", e.user, this);
        }
    }

    this.updateHeader();
}

CIRCChannel.prototype.onKick =
function my_ckick (e)
{
    if (userIsMe (e.lamer))
    {
        this.display (getMsg(MSG_YOURE_GONE,
                             [e.channel.unicodeName, e.user.properNick,
                              e.reason]),
                      "KICK", e.user, this);
        
        /* Try 1 re-join attempt if allowed. */
        if (this.prefs["autoRejoin"])
            this.join(this.mode.key);
    }
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
        
        this.display (getMsg(MSG_SOMEONE_GONE,
                             [e.lamer.properNick, e.channel.unicodeName, 
                              enforcerProper, e.reason]), "KICK", e.user, this);
    }
    
    this._removeUserFromGraph(e.lamer);

    this.updateHeader();
}

CIRCChannel.prototype.onChanMode =
function my_cmode (e)
{
    if ("user" in e)
    {
        var msg = e.decodeParam(1) + " " + e.params.slice(2).join(" ");
        this.display (getMsg(MSG_MODE_CHANGED, [msg, e.user.properNick]),
                      "MODE", e.user, this);
    }

    for (var u in e.usersAffected)
        e.usersAffected[u].updateGraphResource();

    this.updateHeader();
    updateTitle(this);
    if (client.currentObject == this)
        updateUserList();
}

CIRCChannel.prototype.onNick =
function my_cnick (e)
{
    if (userIsMe (e.user))
    {
        if (getTabForObject(this))
        {
            this.displayHere(getMsg(MSG_NEWNICK_YOU, e.user.properNick),
                             "NICK", "ME!", e.user, this);
        }
        this.parent.parent.updateHeader();
    }
    else
    {
        this.display(getMsg(MSG_NEWNICK_NOTYOU, [e.oldNick, e.user.properNick]),
                     "NICK", e.user, this);
    }

    e.user.updateGraphResource();
    updateUserList();
}

CIRCChannel.prototype.onQuit =
function my_cquit (e)
{
    if (userIsMe(e.user))
    {
        /* I dont think this can happen */
        this.display (getMsg(MSG_YOU_QUIT, [e.server.parent.name, e.reason]),
                      "QUIT", e.user, this);
    }
    else
    {
        this.display (getMsg(MSG_SOMEONE_QUIT,
                             [e.user.properNick, e.server.parent.name,
                              e.reason]),
                      "QUIT", e.user, this);
    }

    this._removeUserFromGraph(e.user);

    this.updateHeader();
}

CIRCUser.prototype.onInit =
function user_oninit ()
{
    this.logFile = null;
}

CIRCUser.prototype.onPrivmsg =
function my_cprivmsg(e)
{
    if (!("messages" in this))
    {
        var limit = client.prefs["newTabLimit"];
        if (limit == 0 || client.viewsArray.length < limit)
            openQueryTab(e.server, e.user.nick);
    }
    
    this.display(e.decodeParam(2), "PRIVMSG", e.user, e.server.me);
}

CIRCUser.prototype.onNick =
function my_unick (e)
{
    if (userIsMe(e.user))
    {
        this.parent.parent.updateHeader();
        updateTitle();
    }
    else if ("messages" in this && this.messages)
    {
        this.display(getMsg(MSG_NEWNICK_NOTYOU, [e.oldNick, e.user.properNick]),
                     "NICK", e.user, this);
    }

    this.updateHeader();
    var tab = getTabForObject(this);
    if (tab)
        tab.setAttribute("label", this.properNick);
}

CIRCUser.prototype.onNotice =
function my_notice (e)
{
    this.display(e.decodeParam(2), "NOTICE", this, e.server.me);
}

CIRCUser.prototype.onCTCPAction =
function my_uaction(e)
{
    if (!("messages" in this))
    {
        var limit = client.prefs["newTabLimit"];
        if (limit == 0 || client.viewsArray.length < limit)
            openQueryTab(e.server, e.user.nick);
    }
    
    this.display(e.CTCPData, "ACTION", this, e.server.me);
}

CIRCUser.prototype.onUnknownCTCP =
function my_unkctcp (e)
{
    this.parent.parent.display (getMsg(MSG_UNKNOWN_CTCP,
                                       [e.CTCPCode, e.CTCPData,
                                        e.user.properNick]),
                                "BAD-CTCP", this, e.server.me);
}

CIRCUser.prototype.onDCCChat =
function my_dccchat(e)
{
    if (!jsenv.HAS_SERVER_SOCKETS || !client.prefs["dcc.enabled"])
        return;
    
    this.parent.parent.display(getMsg(MSG_DCCCHAT_GOT_REQUEST, [e.user.properNick, e.host, e.port]), "INFO");
    
    var u = client.dcc.addUser(e.user, e.host);
    var c = client.dcc.addChat(u, e.port);
    
    // Pass the event over to the DCC Chat object.
    e.set = "dcc-chat";
    e.destObject = c;
    e.destMethod = "onGotRequest";
}

CIRCUser.prototype.onDCCSend =
function my_dccchat(e)
{
    if (!jsenv.HAS_SERVER_SOCKETS || !client.prefs["dcc.enabled"])
        return;
    
    this.parent.parent.display(getMsg(MSG_DCCFILE_GOT_REQUEST, [e.user.properNick, e.host, e.port, e.file, e.size]), "INFO");
    
    var u = client.dcc.addUser(e.user, e.host);
    var f = client.dcc.addFileTransfer(u, e.port, e.file, e.size);
    
    // Pass the event over to the DCC Chat object.
    e.set = "dcc-file";
    e.destObject = f;
    e.destMethod = "onGotRequest";
}

CIRCUser.prototype.onDCCReject =
function my_dccchat(e)
{
    if (!client.prefs["dcc.enabled"])
        return;
    
    //FIXME: Uh... cope. //
    
    // Pass the event over to the DCC Chat object.
    //e.set = "dcc-file";
    //e.destObject = f;
    //e.destMethod = "onGotReject";
}

CIRCDCCChat.prototype.onInit =
function my_dccinit(e)
{
    //FIXME//
    this.prefs = client.prefs;
}

CIRCDCCChat.prototype.onPrivmsg =
function my_dccprivmsg(e)
{
    this.displayHere(e.line, "PRIVMSG", e.user, "ME!");
}

CIRCDCCChat.prototype.onCTCPAction =
function my_uaction(e)
{
    this.displayHere(e.CTCPData, "ACTION", e.user, "ME!");
}

CIRCDCCChat.prototype.onUnknownCTCP =
function my_unkctcp(e)
{
    this.displayHere(getMsg(MSG_UNKNOWN_CTCP, [e.CTCPCode, e.CTCPData,
                                               e.user.properNick]),
                     "BAD-CTCP", e.user, "ME!");
}

CIRCDCCChat.prototype.onConnect =
function my_dccdisconnect(e)
{
    playEventSounds("dccchat", "connect");
    this.displayHere(getMsg(MSG_DCCCHAT_OPENED, 
                     [this.user.displayName, this.localIP, this.port]), 
                     "DCC-CHAT");
}

CIRCDCCChat.prototype.onAbort =
function my_dccabort(e)
{
    this.display(getMsg(MSG_DCCCHAT_ABORTED,
                        [this.user.displayName, this.localIP, this.port]), 
                        "DCC-CHAT");
}

CIRCDCCChat.prototype.onDisconnect =
function my_dccdisconnect(e)
{
    playEventSounds("dccchat", "disconnect");
    this.display(getMsg(MSG_DCCCHAT_CLOSED,
                 [this.user.displayName, this.localIP, this.port]), 
                 "DCC-CHAT");
}


CIRCDCCFileTransfer.prototype.onConnect =
function my_dccfileconnect(e)
{
    this.display(getMsg(MSG_DCCFILE_OPENED,
                        [this.user.displayName, this.localIP, this.port]), 
                        "DCC-FILE");
}

CIRCDCCFileTransfer.prototype.onAbort =
function my_dccfileabort(e)
{
    this.display(getMsg(MSG_DCCFILE_ABORTED,
                        [this.user.displayName, this.localIP, this.port]), 
                        "DCC-FILE");
}

CIRCDCCFileTransfer.prototype.onDisconnect =
function my_dccfiledisconnect(e)
{
    this.display(getMsg(MSG_DCCFILE_CLOSED,
                        [this.user.displayName, this.localIP, this.port]), 
                        "DCC-FILE");
}

