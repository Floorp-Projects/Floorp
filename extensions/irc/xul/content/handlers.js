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

function showErrorDlg(message)
{
    const XUL_MIME = "application/vnd.mozilla.xul+xml";
    const XUL_KEY = "http://www.mozilla.org/keymaster/" +
                    "gatekeeper/there.is.only.xul";

    const TITLE = "ChatZilla run-time error";
    const HEADER = "There was a run-time error with ChatZilla. " +
                   "Please report the following information:";

    const OL_JS = "document.getElementById('tb').value = '%S';";
    const TB_STYLE = ' multiline="true" readonly="true"' +
                     ' style="width: 60ex; height: 20em;"';

    const ERROR_DLG = '<?xml version="1.0"?>' +
          '<?xml-stylesheet href="chrome://global/skin/" type="text/css"?>' +
          '<dialog xmlns="' + XUL_KEY + '" buttons="accept" ' +
          'title="' + TITLE + '" onload="' + OL_JS + '">' +
          '<label>' + HEADER + '</label><textbox' + TB_STYLE + ' id="tb"/>' +
          '</dialog>';

    var content = message.replace(/\n/g, "\\n");
    content = content.replace(/'/g, "\\'").replace(/"/g, "&quot;");
    content = content.replace(/</g, "&lt;").replace(/>/g, "&gt;");
    content = ERROR_DLG.replace("%S", content);
    content = encodeURIComponent(content);
    content = "data:" + XUL_MIME + "," + content;

    setTimeout(function() {
                   window.openDialog(content, "_blank", "chrome,modal");
               }, 100);
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
        showErrorDlg(formatException(ex) + "\n" + dumpObjectTree(ex));
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
    if ((e.which != 1) && (e.which != 2))
        return;

    var tbi = document.getElementById (id);
    var view = client.viewsArray[tbi.getAttribute("viewKey")];

    if (e.which == 2)
    {
        dispatch("hide", { view: view.source });
        return;
    }
    else if (e.which == 1)
    {
        dispatch("set-current-view", { view: view.source });
    }
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
        dispatch("focus-input");
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
        var word = line.substring(wordStart, wordEnd);
        var wordLower = word.toLowerCase();
        var d = getObjectDetails(client.currentObject);
        if (d.server)
            wordLower = d.server.toLowerCase(word);

        var co = client.currentObject;

        // We need some special knowledge of how to lower-case strings.
        var lcFn;
        if ("getLCFunction" in co)
            lcFn = co.getLCFunction();

        var matches = co.performTabMatch(line, wordStart, wordEnd, wordLower,
                                         selStart, lcFn);
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
                match = getCommonPfx(matches, lcFn);
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
                dispatch("set-current-view", { view: client.viewsArray[idx].source });
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

var lastWhoCheckTime = new Date();
var lastWhoCheckChannel = null;

function onWhoTimeout()
{
    lastWhoCheckTime = new Date();
    var checkNext = (lastWhoCheckChannel == null);

    for (var n in client.networks)
    {
        var net = client.networks[n];
        if (net.isConnected())
        {
            for (var c in net.primServ.channels)
            {
                var chan = net.primServ.channels[c];

                if (checkNext && chan.active &&
                    chan.getUsersLength() < client.prefs["autoAwayCap"])
                {
                    net.primServ.LIGHTWEIGHT_WHO = true;
                    net.primServ.sendData("WHO " + chan.encodedName + "\n");
                    lastWhoCheckChannel = chan;
                    return;
                }

                if (chan == lastWhoCheckChannel)
                    checkNext = true;
            }
        }
    }

    if (lastWhoCheckChannel)
    {
        lastWhoCheckChannel = null;
        onWhoTimeout();
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
            if (el.nodeName == "textbox")
            {
                popup.showPopup(el, -1, -1, "tooltip", "topleft", "bottomleft");
            }
            else
            {
                var box = el.ownerDocument.getBoxObjectFor(el);
                var pos = { x: client.mainWindow.screenX + box.screenX + 5,
                            y: client.mainWindow.screenY + box.screenY + box.height + 25 };
                popup.moveTo(pos.x, pos.y);
                popup.showPopup(el, 0, 0, "tooltip");
            }
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

CIRCServer.prototype.CTCPHelpClientinfo =
function serv_ccinfohelp()
{
    return MSG_CTCPHELP_CLIENTINFO;
}

CIRCServer.prototype.CTCPHelpAction =
function serv_ccinfohelp()
{
    return MSG_CTCPHELP_ACTION;
}

CIRCServer.prototype.CTCPHelpTime =
function serv_ccinfohelp()
{
    return MSG_CTCPHELP_TIME;
}

CIRCServer.prototype.CTCPHelpVersion =
function serv_ccinfohelp()
{
    return MSG_CTCPHELP_VERSION;
}

CIRCServer.prototype.CTCPHelpOs =
function serv_oshelp()
{
    return MSG_CTCPHELP_OS;
}

CIRCServer.prototype.CTCPHelpHost =
function serv_hosthelp()
{
    return MSG_CTCPHELP_HOST;
}

CIRCServer.prototype.CTCPHelpPing =
function serv_ccinfohelp()
{
    return MSG_CTCPHELP_PING;
}

CIRCServer.prototype.CTCPHelpDcc =
function serv_ccinfohelp()
{
    return MSG_CTCPHELP_DCC;
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
        e.user = new CIRCUser(e.server, null, e.params[2]);

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
        if (arrayIndexOf(e.server.channelTypes, e.params[0][0]) != -1)
        {
            // Message about a channel (e.g. join failed).
            e.channel = new CIRCChannel(e.server, null, e.params[0]);
            if (e.channel.busy)
            {
                e.channel.busy = false;
                updateProgress();
            }
        }
        else
        {
            // Network type error?
            if (this.busy)
            {
                this.busy = false;
                updateProgress();
            }
        }
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
CIRCNetwork.prototype.on422 = /* no MOTD */
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
                var delayFn = function(t) {
                    // This is the quickest way to get out host/IP.
                    t.pendingUserhostReply = true;
                    t.primServ.sendData("USERHOST " +
                                        t.primServ.me.encodedName + "\n");
                };
                setTimeout(delayFn, 1000 * Math.random(), this);
            }

            str = e.decodeParam(2);
            if ("onLogin" in this)
            {
                ev = new CEvent("network", "login", this, "onLogin");
                client.eventPump.addEvent(ev);
            }
            break;

        case "376": /* end of MOTD */
        case "422": /* no MOTD */
            this.busy = false;
            updateProgress();
            /* no break */

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
                         toUnicode(e.CTCPData, this), e.user.unicodeName]),
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
        var me = new RegExp("^" + this.primServ.me.encodedName + "\\*?=", "i");
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
                setTimeout(checkEndList, 5000, network);
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
        display (getMsg(MSG_LIST_REROUTED, this.unicodeName));
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

    if ("pendingWhoReply" in this)
        this.display(getMsg(MSG_WHO_END, [e.params[2], matches]), e.code);

    if ("whoUpdates" in this)
    {
        for (var c in this.whoUpdates)
        {
            for (var i = 0; i < this.whoUpdates[c].length; i++)
                this.whoUpdates[c][i].updateGraphResource();
            this.primServ.channels[c].updateUsers(this.whoUpdates[c]);
        }
        delete this.whoUpdates;
    }

    delete this.pendingWhoReply;
    delete e.server.LIGHTWEIGHT_WHO;
    delete this.whoMatches;
}

CIRCNetwork.prototype.on352 =
function my_352 (e)
{
    //0-352 1-rginda_ 2-#chatzilla 3-chatzilla 4-h-64-236-139-254.aoltw.net
    //5-irc.mozilla.org 6-rginda 7-H
    if ("pendingWhoReply" in this)
    {
        var status;
        if (e.user.isAway)
            status = MSG_GONE;
        else
            status = MSG_HERE;

        this.display(getMsg(MSG_WHO_MATCH,
                            [e.params[6], e.params[3], e.params[4],
                             e.user.desc, status, e.decodeParam(2),
                             e.params[5], e.user.hops]), e.code, e.user);
    }

    updateTitle(e.user);
    if ("whoMatches" in this)
        ++this.whoMatches;
    else
        this.whoMatches = 1;

    if (!("whoUpdates" in this))
        this.whoUpdates = new Object();

    if (e.userHasChanges)
    {
        for (var c in e.server.channels)
        {
            var chan = e.server.channels[c];
            if (chan.active && (e.user.canonicalName in chan.users))
            {
                if (!(c in this.whoUpdates))
                    this.whoUpdates[c] = new Array();
                this.whoUpdates[c].push(chan.users[e.user.canonicalName]);
            }
        }
    }
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
    {
        user = e.user;
        nick = user.unicodeName;
    }

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

    if (e.user)
        e.user.display(text, e.code);
    else
        this.display(text, e.code);
}

CIRCNetwork.prototype.on330 = /* ircu's 330 numeric ("X is logged in as Y") */
function my_330 (e)
{
    this.display (getMsg(MSG_FMT_LOGGED_ON, [e.decodeParam(2), e.params[3]]),
                  "330");
}

CIRCNetwork.prototype.on341 = /* invite reply */
function my_341 (e)
{
    this.display (getMsg(MSG_YOU_INVITE, [e.decodeParam(2), e.decodeParam(3)]),
                  "341");
}

CIRCNetwork.prototype.onInvite = /* invite message */
function my_invite (e)
{
    this.display (getMsg(MSG_INVITE_YOU, [e.user.unicodeName, e.user.name,
                                          e.user.host, e.params[2]]), "INVITE");
}

CIRCNetwork.prototype.on433 = /* nickname in use */
function my_433 (e)
{
    if ((e.params[2] == this.INITIAL_NICK) && (this.state == NET_CONNECTING))
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
    this.busy = true;
    updateProgress();

    this.display (getMsg(MSG_CONNECTION_ATTEMPT,
                         [this.getURL(), e.server.getURL(), e.connectAttempt,
                          this.MAX_CONNECT_ATTEMPTS]), "INFO");
}

CIRCNetwork.prototype.onError =
function my_neterror (e)
{
    var msg;
    var type = "ERROR";

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

            case JSIRC_ERR_CANCELLED:
                msg = MSG_ERR_CANCELLED;
                type = "INFO";
                break;
        }
    }
    else
        msg = e.params[e.params.length - 1];

    dispatch("sync-header");
    updateTitle();

    this.display(msg, type);
}


CIRCNetwork.prototype.onDisconnect =
function my_netdisconnect (e)
{
    var msg;

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
                             [e.server.hostname, this.getURL(),
                              e.server.getURL()]);
                break;

            default:
                msg = getMsg(MSG_CLOSE_STATUS,
                             [this.getURL(), e.server.getURL(),
                              e.disconnectStatus]);
                break;
        }
    }
    else
    {
        msg = getMsg(MSG_CONNECTION_CLOSED,
                     [this.getURL(), e.server.getURL()]);
    }

    /* If we were only /trying/ to connect, and failed, just put an error on
     * the network tab. If we were actually connected ok, put it on all tabs.
     */
    if (this.state != NET_ONLINE)
    {
        this.busy = false;
        updateProgress();

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

    dispatch("sync-header");
    updateTitle();
    updateProgress();
    if ("userClose" in client && client.userClose &&
        client.getConnectionCount() == 0)
        window.close();
}

CIRCNetwork.prototype.onCTCPReplyPing =
function my_replyping (e)
{
    var delay = formatDateOffset ((new Date() - new Date(Number(e.CTCPData))) /
                                  1000);
    display (getMsg(MSG_PING_REPLY, [e.user.unicodeName, delay]), "INFO",
             e.user, "ME!");
}

CIRCNetwork.prototype.on221 =
CIRCNetwork.prototype.onUserMode =
function my_umode (e)
{
    if ("user" in e && e.user)
        e.user.updateHeader();

    //display(getMsg(MSG_USER_MODE, [e.params[1], e.params[2]]), MT_MODE);
    display(getMsg(MSG_USER_MODE, [e.user.unicodeName, e.params[2]]), MT_MODE);
}

CIRCNetwork.prototype.onNick =
function my_cnick (e)
{
    if (!ASSERT(userIsMe(e.user), "network nick event for third party"))
        return;

    if (getTabForObject(this))
    {
        this.displayHere(getMsg(MSG_NEWNICK_YOU, e.user.unicodeName),
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

/* We want to override the base implementations. */
CIRCChannel.prototype._join = CIRCChannel.prototype.join;
CIRCChannel.prototype._part = CIRCChannel.prototype.part;

CIRCChannel.prototype.join =
function chan_join(key)
{
    var joinFailedFn = function _joinFailedFn(t)
    {
        delete t.joinTimer;
        t.busy = false;
        updateProgress();
    }
    if (!this.joined)
    {
        this.joinTimer = setTimeout(joinFailedFn, 30000, this);
        this.busy = true;
        updateProgress();
    }
    this._join(key);
}

CIRCChannel.prototype.part =
function chan_part(reason)
{
    var partFailedFn = function _partFailedFn(t)
    {
        delete t.partTimer;
        t.busy = false;
        updateProgress();
    }
    this.partTimer = setTimeout(partFailedFn, 30000, this);
    this.busy = true;
    updateProgress();
    this._part(reason);
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
}

/* end of names */
CIRCChannel.prototype.on366 =
function my_366 (e)
{
    if (client.currentObject == this)
        /* hide the tree while we add (possibly tons) of nodes */
        client.rdf.setTreeRoot("user-list", client.rdf.resNullChan);

    client.rdf.clearTargets(this.getGraphResource(), client.rdf.resChanUser);

    var updates = new Array();
    for (var u in this.users)
    {
        this.users[u].updateGraphResource();
        this._addUserToGraph (this.users[u]);
        updates.push(this.users[u]);
    }
    this.addUsers(updates);

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
                                            e.user.unicodeName]),
                  "BAD-CTCP", e.user, this);
}

CIRCChannel.prototype.onJoin =
function my_cjoin (e)
{
    if (!("messages" in this))
        this.displayHere(getMsg(MSG_CHANNEL_OPENED, this.unicodeName), MT_INFO);

    if (userIsMe(e.user))
    {
        this.display (getMsg(MSG_YOU_JOINED, e.channel.unicodeName), "JOIN",
                      e.server.me, this);
        client.globalHistory.addPage(this.getURL());

        if ("joinTimer" in this)
        {
            clearTimeout(this.joinTimer);
            delete this.joinTimer;
            this.busy = false;
            updateProgress();
        }

        /* !-channels are "safe" channels, and get a server-generated prefix.
         * For this reason, creating the channel is delayed until this point.
         */
        if (e.channel.unicodeName[0] == "!")
            dispatch("set-current-view", { view: e.channel });
    }
    else
    {
        this.display(getMsg(MSG_SOMEONE_JOINED,
                            [e.user.unicodeName, e.user.name, e.user.host,
                             e.channel.unicodeName]),
                     "JOIN", e.user, this);
    }

    this._addUserToGraph(e.user);
    /* We don't want to add ourself here, since the names reply we'll be
     * getting right after the join will include us as well! (FIXME)
     */
    if (!userIsMe(e.user))
        this.addUsers([e.user]);
    updateUserList()
    this.updateHeader();
}

CIRCChannel.prototype.onPart =
function my_cpart (e)
{
    this._removeUserFromGraph(e.user);
    this.removeUsers([e.user]);
    this.updateHeader();

    if (userIsMe (e.user))
    {
        this.display (getMsg(MSG_YOU_LEFT, e.channel.unicodeName), "PART",
                      e.user, this);
        if (client.currentObject == this)
            /* hide the tree while we remove (possibly tons) of nodes */
            client.rdf.setTreeRoot("user-list", client.rdf.resNullChan);

        client.rdf.clearTargets(this.getGraphResource(),
                                client.rdf.resChanUser, true);

        if ("partTimer" in this)
        {
            clearTimeout(this.partTimer);
            delete this.partTimer;
            this.busy = false;
            updateProgress();
        }

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
                                 [e.user.unicodeName, e.channel.unicodeName,
                                  e.reason]),
                          "PART", e.user, this);
        }
        else
        {
            this.display (getMsg(MSG_SOMEONE_LEFT,
                                 [e.user.unicodeName, e.channel.unicodeName]),
                          "PART", e.user, this);
        }
    }
}

CIRCChannel.prototype.onKick =
function my_ckick (e)
{
    if (userIsMe (e.lamer))
    {
        if (e.user)
        {
            this.display (getMsg(MSG_YOURE_GONE,
                                 [e.channel.unicodeName, e.user.unicodeName,
                                  e.reason]),
                          "KICK", e.user, this);
        }
        else
        {
            this.display (getMsg(MSG_YOURE_GONE,
                                 [e.channel.unicodeName, MSG_SERVER,
                                  e.reason]),
                          "KICK", (void 0), this);
        }

        /* Try 1 re-join attempt if allowed. */
        if (this.prefs["autoRejoin"])
            this.join(this.mode.key);
    }
    else
    {
        var enforcerProper, enforcerNick;
        if (e.user && userIsMe(e.user))
        {
            enforcerProper = "YOU";
            enforcerNick = "ME!";
        }
        else if (e.user)
        {
            enforcerProper = e.user.unicodeName;
            enforcerNick = e.user.encodedName;
        }
        else
        {
            enforcerProper = MSG_SERVER;
            enforcerNick = MSG_SERVER;
        }

        this.display (getMsg(MSG_SOMEONE_GONE,
                             [e.lamer.unicodeName, e.channel.unicodeName,
                              enforcerProper, e.reason]), "KICK", e.user, this);
    }

    this._removeUserFromGraph(e.lamer);
    this.removeUsers([e.lamer]);
    this.updateHeader();
}

CIRCChannel.prototype.onChanMode =
function my_cmode (e)
{
    if ("user" in e)
    {
        var msg = e.decodeParam(1);
        for (var i = 2; i < e.params.length; i++)
            msg += " " + e.decodeParam(i);

        this.display (getMsg(MSG_MODE_CHANGED, [msg, e.user.unicodeName]),
                      "MODE", e.user, this);
    }

    var updates = new Array();
    for (var u in e.usersAffected)
    {
        e.usersAffected[u].updateGraphResource();
        updates.push(e.usersAffected[u]);
    }
    this.updateUsers(updates);

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
            this.displayHere(getMsg(MSG_NEWNICK_YOU, e.user.unicodeName),
                             "NICK", "ME!", e.user, this);
        }
        this.parent.parent.updateHeader();
    }
    else
    {
        this.display(getMsg(MSG_NEWNICK_NOTYOU, [e.oldNick, e.user.unicodeName]),
                     "NICK", e.user, this);
    }

    e.user.updateGraphResource();
    //this.updateUsers([e.user]);
    /* updateUsers isn't clever enough (currently) to handle a nick change, so
     * we fake the user leaving (with the old nick) and coming back (with the
     * new nick).
     */
    this.removeUsers([e.server.addUser(e.oldNick)]);
    this.addUsers([e.user]);
    updateUserList();
}

CIRCChannel.prototype.onQuit =
function my_cquit (e)
{
    if (userIsMe(e.user))
    {
        /* I dont think this can happen */
        this.display (getMsg(MSG_YOU_QUIT, [e.server.parent.unicodeName, e.reason]),
                      "QUIT", e.user, this);
    }
    else
    {
        this.display (getMsg(MSG_SOMEONE_QUIT,
                             [e.user.unicodeName, e.server.parent.unicodeName,
                              e.reason]),
                      "QUIT", e.user, this);
    }

    this._removeUserFromGraph(e.user);
    this.removeUsers([e.user]);
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
            openQueryTab(e.server, e.user.unicodeName);
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
        this.display(getMsg(MSG_NEWNICK_NOTYOU, [e.oldNick, e.user.unicodeName]),
                     "NICK", e.user, this);
    }

    this.updateHeader();
    var tab = getTabForObject(this);
    if (tab)
        tab.setAttribute("label", this.unicodeName);
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
            openQueryTab(e.server, e.user.unicodeName);
    }

    this.display(e.CTCPData, "ACTION", this, e.server.me);
}

CIRCUser.prototype.onUnknownCTCP =
function my_unkctcp (e)
{
    this.parent.parent.display (getMsg(MSG_UNKNOWN_CTCP,
                                       [e.CTCPCode, e.CTCPData,
                                        e.user.unicodeName]),
                                "BAD-CTCP", this, e.server.me);
}

CIRCUser.prototype.onDCCChat =
function my_dccchat(e)
{
    if (!jsenv.HAS_SERVER_SOCKETS || !client.prefs["dcc.enabled"])
        return;

    var u = client.dcc.addUser(e.user, e.host);
    var c = client.dcc.addChat(u, e.port);

    client.munger.entries[".inline-buttons"].enabled = true;
    var cmds = getMsg(MSG_DCC_COMMAND_ACCEPT, "dcc-accept " + c.id) + " " +
               getMsg(MSG_DCC_COMMAND_DECLINE, "dcc-decline " + c.id);
    this.parent.parent.display(getMsg(MSG_DCCCHAT_GOT_REQUEST,
                                      [e.user.unicodeName, e.host, e.port, cmds]),
                               "DCC-CHAT");
    client.munger.entries[".inline-buttons"].enabled = false;

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

    var u = client.dcc.addUser(e.user, e.host);
    var f = client.dcc.addFileTransfer(u, e.port, e.file, e.size);

    client.munger.entries[".inline-buttons"].enabled = true;
    var cmds = getMsg(MSG_DCC_COMMAND_ACCEPT, "dcc-accept " + f.id) + " " +
               getMsg(MSG_DCC_COMMAND_DECLINE, "dcc-decline " + f.id);
    this.parent.parent.display(getMsg(MSG_DCCFILE_GOT_REQUEST,
                                      [e.user.unicodeName, e.host, e.port,
                                       e.file, e.size, cmds]),
                               "DCC-FILE");
    client.munger.entries[".inline-buttons"].enabled = false;

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
    /* FIXME: we're currently 'borrowing' the client views' prefs until we have
     * our own pref manager.
     */
    this.prefs = client.prefs;
}

CIRCDCCChat.prototype._getParams =
function my_dccgetparams()
{
    return [this.user.unicodeName, this.localIP, this.port];
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
                                               e.user.unicodeName]),
                     "BAD-CTCP", e.user, "ME!");
}

CIRCDCCChat.prototype.onConnect =
function my_dccdisconnect(e)
{
    playEventSounds("dccchat", "connect");
    this.displayHere(getMsg(MSG_DCCCHAT_OPENED, this._getParams()), "DCC-CHAT");
}

CIRCDCCChat.prototype.onAbort =
function my_dccabort(e)
{
    this.display(getMsg(MSG_DCCCHAT_ABORTED, this._getParams()), "DCC-CHAT");
}

CIRCDCCChat.prototype.onFail =
function my_dccfail(e)
{
    this.display(getMsg(MSG_DCCCHAT_FAILED, this._getParams()), "DCC-CHAT");
}

CIRCDCCChat.prototype.onDisconnect =
function my_dccdisconnect(e)
{
    playEventSounds("dccchat", "disconnect");
    this.display(getMsg(MSG_DCCCHAT_CLOSED, this._getParams()), "DCC-CHAT");
}


CIRCDCCFileTransfer.prototype.onInit =
function my_dccfileinit(e)
{
    /* FIXME: we're currently 'borrowing' the client views' prefs until we have
     * our own pref manager.
     */
    this.prefs = client.prefs;
    this.busy = false;
    this.progress = -1;
    updateProgress();
}

CIRCDCCFileTransfer.prototype._getParams =
function my_dccfilegetparams()
{
    var dir = MSG_UNKNOWN;

    if (this.state.dir == DCC_DIR_GETTING)
        dir = MSG_DCCLIST_FROM;

    if (this.state.dir == DCC_DIR_SENDING)
        dir = MSG_DCCLIST_TO;

    return [this.filename, dir, this.user.unicodeName,
            this.localIP, this.port];
}

CIRCDCCFileTransfer.prototype.onConnect =
function my_dccfileconnect(e)
{
    this.displayHere(getMsg(MSG_DCCFILE_OPENED, this._getParams()), "DCC-FILE");
    this.busy = true;
    this.progress = 0;
    updateProgress();
    this._lastUpdate = new Date();
    this._lastPosition = 0;
}

CIRCDCCFileTransfer.prototype.onProgress =
function my_dccfileprogress(e)
{
    var now = new Date();
    var pcent = Math.floor(100 * this.position / this.size);

    // If we've moved 100KiB or waited 10s, update the progress bar.
    if ((this.position > this._lastPosition + 102400) ||
        (now - this._lastUpdate > 10000))
    {
        this.progress = pcent;
        updateProgress();

        var tab = getTabForObject(this);
        if (tab)
            tab.setAttribute("label", this.viewName + " (" + pcent + "%)");

        this.updateHeader();

        this._lastPosition = this.position;
    }

    // If it's also been 10s or more since we last displayed a msg...
    if (now - this._lastUpdate > 10000)
    {
        this.displayHere(getMsg(MSG_DCCFILE_PROGRESS,
                                [pcent, this.position, this.size]),
                         "DCC-FILE");

        this._lastUpdate = now;
    }
}

CIRCDCCFileTransfer.prototype.onAbort =
function my_dccfileabort(e)
{
    this.busy = false;
    updateProgress();
    this.display(getMsg(MSG_DCCFILE_ABORTED, this._getParams()), "DCC-FILE");
}

CIRCDCCFileTransfer.prototype.onFail =
function my_dccfilefail(e)
{
    this.busy = false;
    updateProgress();
    this.display(getMsg(MSG_DCCFILE_FAILED, this._getParams()), "DCC-FILE");
}

CIRCDCCFileTransfer.prototype.onDisconnect =
function my_dccfiledisconnect(e)
{
    this.busy = false;
    updateProgress();
    this.updateHeader();

    var tab = getTabForObject(this);
    if (tab)
        tab.setAttribute("label", this.viewName + " (DONE)");

    this.display(getMsg(MSG_DCCFILE_CLOSED, this._getParams()), "DCC-FILE");
}

