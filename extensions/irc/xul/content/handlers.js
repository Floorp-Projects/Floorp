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
 *   Shawn Wilsher <me@shawnwilsher.com>
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
    node = document.getElementById("security-button");
    node.addEventListener("dblclick", onSecurityIconDblClick, false);

    window.onkeypress = onWindowKeyPress;

    window.isFocused = false;
    window.addEventListener("focus", onWindowFocus, true);
    window.addEventListener("blur", onWindowBlue, true);

    client.inputPopup = null;

    // Should fail silently pre-moz1.4
    doCommandWithParams("cmd_clipboardDragDropHook",
                        {addhook: CopyPasteHandler});
}

function onClose()
{
    if ("userClose" in client && client.userClose)
        return true;

    // if we're not connected to anything, just close the window
    if (!("getConnectionCount" in client) || (client.getConnectionCount() == 0))
        return true;

    /* otherwise, try to close out gracefully */
    client.wantToQuit();
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
function onTabClick(e, id)
{
    if (e.which != 2)
        return;

    var tbi = document.getElementById(id);
    var view = client.viewsArray[tbi.getAttribute("viewKey")];

    if (e.which == 2)
    {
        dispatch("hide", { view: view.source });
        return;
    }
}

function onTabSelect(e)
{
    var tabs = e.target;

    /* Hackaround, bug 314230. XBL likes focusing a tab before onload has fired.
     * That means the tab we're trying to select here will be the hidden one,
     * which doesn't have a viewKey. We catch that case.
     */
    if (!tabs.selectedItem.hasAttribute("viewKey"))
        return;

    var key = tabs.selectedItem.getAttribute("viewKey");
    var view = client.viewsArray[key];
    dispatch("set-current-view", {view:view.source});
}

function onMessageViewClick(e)
{
    if ((e.which != 1) && (e.which != 2))
        return true;

    var cx = getMessagesContext(null, e.target);
    var command = getEventCommand(e);
    if (!client.commandManager.isCommandSatisfied(cx, command))
        return false;

    dispatch(command, cx);
    dispatch("focus-input");
    e.preventDefault();
    return true;
}

function onMessageViewMouseDown(e)
{
    if ((typeof startScrolling != "function") ||
        ((e.which != 1) && (e.which != 2)))
    {
        return true;
    }
        
    var cx = getMessagesContext(null, e.target);
    var command = getEventCommand(e);
    if (!client.commandManager.isCommandSatisfied(cx, command))
        startScrolling(e);
    return true;
}

function getEventCommand(e)
{
    if (e.which == 2)
        return client.prefs["messages.middleClick"];
    if (e.metaKey || e.altKey)
        return client.prefs["messages.metaClick"];
    if (e.ctrlKey)
        return client.prefs["messages.ctrlClick"];
    
    return client.prefs["messages.click"];
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

function onSecurityIconDblClick(e)
{
    if (e.button == 0)
        displayCertificateInfo();
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
            if (e.ctrlKey)
                e.line = client.COMMAND_CHAR + "say " + e.line;
            if (e.line.search(/\S/) == -1)
                return;
            onInputCompleteLine (e);
            break;

        case 37: /* left */
             if (e.altKey && e.metaKey)
                 cycleView(-1);
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

        case 39: /* right */
             if (e.altKey && e.metaKey)
                 cycleView(+1);
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

        case 33: /* pgup */
            if (e.ctrlKey)
            {
                cycleView(-1);
                e.preventDefault();
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
                e.preventDefault();
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

    // Code is zero if we have an alphanumeric being given to us in the event.
    if (code != 0)
      return;

    // The following code is copied from:
    //   /mozilla/browser/base/content/browser.js
    //   Revision: 1.748
    //   Lines: 1397-1421

    // \d in a RegExp will find any Unicode character with the "decimal digit"
    // property (Nd)
    var regExp = /\d/;
    if (!regExp.test(String.fromCharCode(e.charCode)))
        return;

    // Some Unicode decimal digits are in the range U+xxx0 - U+xxx9 and some are
    // in the range U+xxx6 - U+xxxF. Find the digit 1 corresponding to our
    // character.
    var digit1 = (e.charCode & 0xFFF0) | 1;
    if (!regExp.test(String.fromCharCode(digit1)))
      digit1 += 6;

    var idx = e.charCode - digit1;

    const isMac     = client.platform == "Mac";
    const isLinux   = client.platform == "Linux";
    const isWindows = client.platform == "Windows";
    const isOS2     = client.platform == "OS/2";
    const isUnknown = !(isMac || isLinux || isWindows || isOS2);
    const isSuite   = client.host == "Mozilla";

    if ((0 <= idx) && (idx <= 8))
    {
        if ((!isSuite && isMac && e.metaKey) ||
            (!isSuite && (isLinux || isOS2) && e.altKey) ||
            (!isSuite && (isWindows || isUnknown) && e.ctrlKey) ||
            (isSuite && e.altKey))
        {
            // Pressing 1-8 takes you to that tab, while pressing 9 takes you
            // to the last tab always.
            if (idx == 8)
                idx = client.viewsArray.length - 1;

            if ((idx in client.viewsArray) && client.viewsArray[idx].source)
            {
                var newView = client.viewsArray[idx].source;
                dispatch("set-current-view", { view: newView });
            }
            e.preventDefault();
            return;
        }
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

function onNotifyTimeout()
{
    /* Workaround: bug 318419 - timers sometimes fire way too quickly.
     * This catches us out, as it causes the notify code (this) and the who
     * code (below) to fire continuously, which completely floods the
     * sendQueue. We work around this for now by reporting the probable
     * error condition here, but don't attempt to stop it.
     */

    for (var n in client.networks)
    {
        var net = client.networks[n];
        if (net.isConnected()) {
            // WORKAROUND BEGIN //
            if (!("bug318419" in client) &&
                (net.primServ.sendQueue.length >= 1000))
            {
                client.bug318419 = 10;
                display(MSG_BUG318419_WARNING, MT_WARN);
                window.getAttention();
                return;
            }
            else if (("bug318419" in client) && (client.bug318419 > 0) &&
                     (net.primServ.sendQueue.length >= (1000 * client.bug318419)))
            {
                client.bug318419++;
                display(MSG_BUG318419_ERROR, MT_ERROR);
                window.getAttention();
                return;
            }
            // WORKAROUND END //
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

function onWhoTimeout()
{
    function checkWho()
    {
        var checkNext = (net.lastWhoCheckChannel == null);
        for (var c in net.primServ.channels)
        {
            var chan = net.primServ.channels[c];

            if (checkNext && chan.active &&
                chan.getUsersLength() < client.prefs["autoAwayCap"])
            {
                net.primServ.LIGHTWEIGHT_WHO = true;
                net.primServ.sendData("WHO " + chan.encodedName + "\n");
                net.lastWhoCheckChannel = chan;
                net.lastWhoCheckTime = Number(new Date());
                return;
            }

            if (chan == net.lastWhoCheckChannel)
                checkNext = true;
        }
        if (net.lastWhoCheckChannel)
        {
            net.lastWhoCheckChannel = null;
            checkWho();
        }
    };

    for (var n in client.networks)
    {
        var net = client.networks[n];
        var period = net.prefs["autoAwayPeriod"];
        // The time since the last check, with a 5s error margin to
        // stop us from not checking because the timer fired a tad early:
        var waited = Number(new Date()) - net.lastWhoCheckTime + 5000;
        if (net.isConnected() && (period != 0) && (period * 60000 < waited))
            checkWho();
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

CIRCChannel.prototype._updateConferenceMode =
function my_updateconfmode()
{
    const minDiff = client.CONFERENCE_LOW_PASS;

    var enabled   = this.prefs["conference.enabled"];
    var userLimit = this.prefs["conference.limit"];
    var userCount = this.getUsersLength();

    if (userLimit == 0)
    {
        // userLimit == 0 --> always off.
        if (enabled)
            this.prefs["conference.enabled"] = false;
    }
    else if (userLimit == 1)
    {
        // userLimit == 1 --> always on.
        if (!enabled)
            this.prefs["conference.enabled"] = true;
    }
    else if (enabled && (userCount < userLimit - minDiff))
    {
        this.prefs["conference.enabled"] = false;
    }
    else if (!enabled && (userCount > userLimit + minDiff))
    {
        this.prefs["conference.enabled"] = true;
    }
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

CIRCServer.prototype.CTCPHelpSource =
function serv_csrchelp()
{
    return MSG_CTCPHELP_SOURCE;
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

/**
 * Calculates delay before the next automatic connection attempt.
 *
 * If the number of connection attempts is limited, use fixed interval
 * MIN_RECONNECT_MS. For unlimited attempts (-1), use exponential backoff: the
 * interval between connection attempts to the network (not individual
 * servers) is doubled after each attempt, up to MAX_RECONNECT_MS.
 */
CIRCNetwork.prototype.getReconnectDelayMs =
function my_getReconnectDelayMs()
{
    var nServers = this.serverList.length;

    if ((-1 != this.MAX_CONNECT_ATTEMPTS) ||
        (0 != this.connectCandidate % nServers))
    {
        return this.MIN_RECONNECT_MS;
    }

    var networkRound = Math.ceil(this.connectCandidate / nServers);

    var rv = this.MIN_RECONNECT_MS * Math.pow(2, networkRound - 1);

    // clamp rv between MIN/MAX_RECONNECT_MS
    rv = Math.min(Math.max(rv, this.MIN_RECONNECT_MS), this.MAX_RECONNECT_MS);

    return rv;
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
    var msg = getMsg("msg.irc." + e.code, null, "");
    if (msg)
    {
        if (arrayIndexOf(e.server.channelTypes, e.params[0][0]) != -1)
        {
            // Message about a channel (e.g. join failed).
            e.channel = new CIRCChannel(e.server, null, e.params[0]);
        }

        var targetDisplayObj = this;
        if (e.channel && ("messages" in e.channel))
            targetDisplayObj = e.channel;

        // Check for /knock support for the +i message.
        if (((e.code == 471) || (e.code == 473) || (e.code == 475)) &&
            ("knock" in e.server.servCmds))
        {
            var args = [msg, e.channel.unicodeName,
                        "knock " + e.channel.unicodeName];
            msg = getMsg("msg.irc." + e.code + ".knock", args, "");
            client.munger.getRule(".inline-buttons").enabled = true;
            targetDisplayObj.display(msg);
            client.munger.getRule(".inline-buttons").enabled = false;
        }
        else
        {
            targetDisplayObj.display(msg);
        }

        if (e.channel)
        {
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

CIRCNetwork.prototype.lastWhoCheckChannel = null;
CIRCNetwork.prototype.lastWhoCheckTime = 0;
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
            var c, u;
            // If we've switched servers, *first* we must rehome our objects.
            if (this.lastServer && (this.lastServer != this.primServ))
            {
                for (c in this.lastServer.channels)
                    this.lastServer.channels[c].rehome(this.primServ);
                for (u in this.lastServer.users)
                    this.lastServer.users[u].rehome(this.primServ);

                // This makes sure we have the *right* me object.
                this.primServ.me.rehome(this.primServ);
            }

            // Update the list of ignored users from the prefs:
            var ignoreAry = this.prefs["ignoreList"];
            for (var j = 0; j < ignoreAry.length; ++j)
                this.ignoreList[ignoreAry[j]] = getHostmaskParts(ignoreAry[j]);

            // Update everything.
            // Welcome to history.
            if (client.globalHistory)
                client.globalHistory.addPage(this.getURL());
            updateTitle(this);
            this.updateHeader();
            client.updateHeader();
            updateSecurityIcon();
            updateStalkExpression(this);

            client.ident.removeNetwork(this);

            str = e.decodeParam(2);

            break;

        case "251": /* users */
            var cmdary = this.prefs["autoperform"];
            for (var i = 0; i < cmdary.length; ++i)
            {
                if (cmdary[i][0] == "/")
                    this.dispatch(cmdary[i].substr(1));
                else
                    this.dispatch(cmdary[i]);
            }

            if (this.prefs["away"])
                this.dispatch("away", { reason: this.prefs["away"] });

            if (this.lastServer)
            {
                // Re-join channels from previous connection.
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

            // Had some collision during connect.
            if (this.primServ.me.unicodeName != this.prefs["nickname"])
            {
                this.reclaimLeft = this.RECLAIM_TIMEOUT;
                this.reclaimName();
            }

            if ("onLogin" in this)
            {
                ev = new CEvent("network", "login", this, "onLogin");
                client.eventPump.addEvent(ev);
            }

            str = e.decodeParam(e.params.length - 1);
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
    client.munger.getRule(".mailto").enabled = client.prefs["munger.mailto"];
    this.display(e.decodeParam(2), "NOTICE", this, e.server.me);
    client.munger.getRule(".mailto").enabled = false;
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
    function lower(text)
    {
        return e.server.toLowerCase(text);
    };

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
        if (!arrayContains(onList, lower(this.prefs["notifyList"][i])))
            /* user is not on */
            offList.push(lower(this.prefs["notifyList"][i]));
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

/* away off reply */
CIRCNetwork.prototype.on305 =
function my_305(e)
{
    this.display(MSG_AWAY_OFF);

    return true;
}

/* away on reply */
CIRCNetwork.prototype.on306 =
function my_306(e)
{
    this.display(getMsg(MSG_AWAY_ON, this.prefs["away"]));

    return true;
}


CIRCNetwork.prototype.on263 = /* 'try again' */
function my_263 (e)
{
    /* Urgh, this one's a pain. We need to abort whatever we tried, and start
     * it again if appropriate.
     *
     * Known causes of this message:
     *   - LIST, with or without a parameter.
     */

    if (("_list" in this) && !this._list.done && (this._list.count == 0))
    {
        // We attempted a LIST, and we think it failed. :)
        this._list.done = true;
        this._list.error = e.decodeParam(2);
        // Return early for this one if we're saving it.
        if ("file" in this._list)
            return true;
    }

    e.destMethod = "onUnknown";
    e.destObject = this;
    return true;
}

CIRCNetwork.prototype.isRunningList =
function my_running_list()
{
    return (("_list" in this) && !this._list.done && !this._list.cancelled);
}

CIRCNetwork.prototype.list =
function my_list(word, file)
{
    const NORMAL_FILE_TYPE = Components.interfaces.nsIFile.NORMAL_FILE_TYPE;

    if (("_list" in this) && !this._list.done)
        return false;

    this._list = new Array();
    this._list.string = word;
    this._list.regexp = null;
    this._list.done = false;
    this._list.count = 0;
    if (file)
    {
        var lfile = new LocalFile(file);
        if (!lfile.localFile.exists())
        {
            // futils.umask may be 0022. Result is 0644.
            lfile.localFile.create(NORMAL_FILE_TYPE, 0666 & ~futils.umask);
        }
        this._list.file = new LocalFile(lfile.localFile, ">");
    }

    if (isinstance(word, RegExp))
    {
        this._list.regexp = word;
        this._list.string = "";
        word = "";
    }

    if (word)
        this.primServ.sendData("LIST " + fromUnicode(word, this) + "\n");
    else
        this.primServ.sendData("LIST\n");

    return true;
}

CIRCNetwork.prototype.listInit =
function my_list_init ()
{
    function checkEndList (network)
    {
        if (network._list.count == network._list.lastLength)
        {
            network.on323();
        }
        else
        {
            network._list.lastLength = network._list.count;
            network._list.endTimeout =
                setTimeout(checkEndList, 5000, network);
        }
    }

    function outputList (network)
    {
        const CHUNK_SIZE = 5;
        var list = network._list;
        if (list.cancelled)
        {
            if (list.done)
            {
                /* The server is no longer throwing stuff at us, so now
                 * we can safely kill the list.
                 */
                network.display(getMsg(MSG_LIST_END,
                                       [list.displayed, list.count]));
                delete network._list;
            }
            else
            {
                /* We cancelled the list, but we're still getting data.
                 * Handle that data, but don't display, and do it more
                 * slowly, so we cause less lag.
                 */
                setTimeout(outputList, 1000, network);
            }
            return;
        }
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
        }
        else
        {
            setTimeout(outputList, 250, network);
        }
    }

    if (!("_list" in this))
    {
        this._list = new Array();
        this._list.string = MSG_UNKNOWN;
        this._list.regexp = null;
        this._list.done = false;
        this._list.count = 0;
    }

    if (!("file" in this._list))
    {
        this._list.displayed = 0;
        if (client.currentObject != this)
            display (getMsg(MSG_LIST_REROUTED, this.unicodeName));
        setTimeout(outputList, 250, this);
    }
    this._list.lastLength = 0;
    this._list.endTimeout = setTimeout(checkEndList, 5000, this);
}

CIRCNetwork.prototype.abortList =
function my_abortList()
{
    this._list.cancelled = true;
}

CIRCNetwork.prototype.on321 = /* LIST reply header */
function my_321 (e)
{
    this.listInit();

    if (!("file" in this._list))
        this.displayHere (e.params[2] + " " + e.params[3], "321");
}

CIRCNetwork.prototype.on323 = /* end of LIST reply */
function my_323 (e)
{
    if (this._list.endTimeout)
    {
        clearTimeout(this._list.endTimeout);
        delete this._list.endTimeout;
    }
    if (("file" in this._list))
        this._list.file.close();

    this._list.done = true;
    this._list.event323 = e;
}

CIRCNetwork.prototype.on322 = /* LIST reply */
function my_listrply (e)
{
    if (!("_list" in this) || !("lastLength" in this._list))
        this.listInit();

    ++this._list.count;

    /* If the list has been cancelled, don't bother adding all this info
     * anymore. Do increase the count (above), otherwise we never truly notice
     * the list being finished.
     */
    if (this._list.cancelled)
        return;

    var chanName = e.decodeParam(2);
    var topic = e.decodeParam(4);
    if (!this._list.regexp || chanName.match(this._list.regexp) ||
        topic.match(this._list.regexp))
    {
        if (!("file" in this._list))
        {
            this._list.push([chanName, e.params[3], topic]);
        }
        else
        {
            this._list.file.write(fromUnicode(chanName, "UTF-8") + " " +
                                  e.params[3] + " " +
                                  fromUnicode(topic, "UTF-8") + "\n");
        }
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
        if (this.whoisList && (target in this.whoisList))
            this.whoisList[target] = false;
        else
            display(toUnicode(e.params[3], this));
    }
}

/* 464; "invalid or missing password", occurs as a reply to both OPER and
 * sometimes initially during user registration. */
CIRCNetwork.prototype.on464 =
function my_464(e)
{
    if (this.state == NET_CONNECTING)
    {
        // If we are in the process of connecting we are needing a login
        // password, subtly different from after user registration.
        this.display(MSG_IRC_464_LOGIN);
    }
    else
    {
        e.destMethod = "onUnknown";
        e.destObject = this;
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

CIRCNetwork.prototype.on301 = /* user away message */
function my_301(e)
{
    if (e.user.awayMessage != e.user.lastShownAwayMessage)
    {
        var params = [e.user.unicodeName, e.user.awayMessage];
        e.user.display(getMsg(MSG_WHOIS_AWAY, params), e.code);
        e.user.lastShownAwayMessage = e.user.awayMessage;
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
    var lowerNick = this.primServ.toLowerCase(nick);
    var user;

    if (this.whoisList && (e.code != 318) && (lowerNick in this.whoisList))
        this.whoisList[lowerNick] = true;

    if (e.user)
    {
        user = e.user;
        nick = user.unicodeName;
    }

    switch (Number(e.code))
    {
        case 311:
            // Clear saved away message so it appears and can be reset.
            if (e.user)
                e.user.lastShownAwayMessage = "";

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
            // If this user isn't here, don't display anything and do a whowas
            if (this.whoisList && (lowerNick in this.whoisList) &&
                !this.whoisList[lowerNick])
            {
                delete this.whoisList[lowerNick];
                this.primServ.whowas(nick, 1);
                return;
            }
            if (this.whoisList)
                delete this.whoisList[lowerNick];

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
    var msg = getMsg(MSG_FMT_LOGGED_ON, [e.decodeParam(2), e.params[3]]);
    if (e.user)
        e.user.display(msg, "330");
    else
        this.display(msg, "330");
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
    this.display(getMsg(MSG_INVITE_YOU, [e.user.unicodeName, e.user.name,
                                         e.user.host, e.channel.unicodeName]),
                 "INVITE");

    if ("messages" in e.channel)
        e.channel.join();
}

CIRCNetwork.prototype.on433 = /* nickname in use */
function my_433 (e)
{
    var nick = toUnicode(e.params[2], this);

    if ("pendingReclaimCheck" in this)
    {
        delete this.pendingReclaimCheck;
        return;
    }

    if (this.state == NET_CONNECTING)
    {
        // Force a number, thanks.
        var nickIndex = 1 * arrayIndexOf(this.prefs["nicknameList"], nick);
        var newnick;

        dd("433: failed with " + nick + " (" + nickIndex + ")");

        var tryList = true;

        if ((("_firstNick" in this) && (this._firstNick == -1)) ||
            (this.prefs["nicknameList"].length == 0) ||
            ((nickIndex != -1) && (this.prefs["nicknameList"].length < 2)))
        {
            tryList = false;
        }

        if (tryList)
        {
            nickIndex = (nickIndex + 1) % this.prefs["nicknameList"].length;

            if (("_firstNick" in this) && (this._firstNick == nickIndex))
            {
                // We're back where we started. Give up with this method.
                this._firstNick = -1;
                tryList = false;
            }
        }

        if (tryList)
        {
            newnick = this.prefs["nicknameList"][nickIndex];
            dd("     trying " + newnick + " (" + nickIndex + ")");

            // Save first index we've tried.
            if (!("_firstNick" in this))
                this._firstNick = nickIndex;
        }
        else
        {
            newnick = this.INITIAL_NICK + "_";
            dd("     trying " + newnick);
        }

        this.INITIAL_NICK = newnick;
        this.display(getMsg(MSG_RETRY_NICK, [nick, newnick]), "433");
        this.primServ.sendData("NICK " + fromUnicode(newnick, this) + "\n");
    }
    else
    {
        this.display(getMsg(MSG_NICK_IN_USE, nick), "433");
    }
}

CIRCNetwork.prototype.onStartConnect =
function my_sconnect (e)
{
    this.busy = true;
    updateProgress();
    if ("_firstNick" in this)
        delete this._firstNick;

    this.display(getMsg(MSG_CONNECTION_ATTEMPT,
                        [this.getURL(), e.server.getURL()]), "INFO");

    if (this.prefs["identd.enabled"])
    {
        if (jsenv.HAS_SERVER_SOCKETS)
            client.ident.addNetwork(this, e.server);
        else
            display(MSG_IDENT_SERVER_NOT_POSSIBLE, MT_WARN);
    }
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
                // error already displayed in onDisconnect
                break;

            case JSIRC_ERR_OFFLINE:
                msg = MSG_ERR_OFFLINE;
                break;

            case JSIRC_ERR_NO_SECURE:
                msg = getMsg(MSG_ERR_NO_SECURE, this.unicodeName);
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

    if (this.state == NET_OFFLINE)
    {
        this.busy = false;
        updateProgress();
    }

    client.ident.removeNetwork(this);

    if (msg)
        this.display(msg, type);

    if (this.deleteWhenDone)
        this.dispatch("delete-view");

    delete this.deleteWhenDone;

}


CIRCNetwork.prototype.onDisconnect =
function my_netdisconnect (e)
{
    var msg, msgNetwork;
    var msgType = "ERROR";

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

            case NS_ERROR_UNKNOWN_PROXY_HOST:
                msg = getMsg(MSG_UNKNOWN_PROXY_HOST,
                             [this.getURL(), e.server.getURL()]);
                break;

            case NS_ERROR_PROXY_CONNECTION_REFUSED:
                msg = MSG_PROXY_CONNECTION_REFUSED;
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

    // e.quitting signals the disconnect was intended: don't use "ERROR".
    if (e.quitting)
    {
        msgType = "DISCONNECT";
        msg = getMsg(MSG_CONNECTION_QUIT, [this.getURL(), e.server.getURL()]);
        msgNetwork = msg;
    }
    else
    {
        var delayStr = formatDateOffset(this.getReconnectDelayMs() / 1000);
        if (this.MAX_CONNECT_ATTEMPTS == -1)
        {
            msgNetwork = getMsg(MSG_RECONNECTING_IN, [msg, delayStr]);
        }
        else if (this.connectAttempt < this.MAX_CONNECT_ATTEMPTS)
        {
            var left = this.MAX_CONNECT_ATTEMPTS - this.connectAttempt;
            if (left == 1)
            {
                msgNetwork = getMsg(MSG_RECONNECTING_IN_LEFT1, [msg, delayStr]);
            }
            else
            {
                msgNetwork = getMsg(MSG_RECONNECTING_IN_LEFT,
                                    [msg, left, delayStr]);
            }
        }
        else
        {
            msgNetwork = getMsg(MSG_CONNECTION_EXHAUSTED, msg);
        }
    }

    /* If we were connected ok, put an error on all tabs. If we were only
     * /trying/ to connect, and failed, just put it on the network tab. 
     */
    if (this.state == NET_ONLINE)
    {
        for (var v in client.viewsArray)
        {
            var obj = client.viewsArray[v].source;
            if (obj == this)
            {
                obj.displayHere(msgNetwork, msgType);
            }
            else if (obj != client)
            {
                var details = getObjectDetails(obj);
                if ("server" in details && details.server == e.server)
                    obj.displayHere(msg, msgType);
            }
        }
    }
    else
    {
        this.busy = false;
        updateProgress();

        // Don't do anything if we're cancelling.
        if (this.state != NET_CANCELLING)
        {
            this.displayHere(msgNetwork, msgType);
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
    updateSecurityIcon();

    client.ident.removeNetwork(this);

    if ("userClose" in client && client.userClose &&
        client.getConnectionCount() == 0)
        window.close();

    if (("reconnect" in this) && this.reconnect)
    {
        this.connect(this.requireSecurity);
        delete this.reconnect;
    }
}

CIRCNetwork.prototype.onCTCPReplyPing =
function my_replyping (e)
{
    // see bug 326523
    if (stringTrim(e.CTCPData).length != 13)
    {
        this.display(getMsg(MSG_PING_REPLY_INVALID, e.user.unicodeName),
                     "INFO", e.user, "ME!");
        return;
    }

    var delay = formatDateOffset((new Date() - new Date(Number(e.CTCPData))) /
                                 1000);
    this.display(getMsg(MSG_PING_REPLY, [e.user.unicodeName, delay]), "INFO",
                 e.user, "ME!");
}

CIRCNetwork.prototype.on221 =
CIRCNetwork.prototype.onUserMode =
function my_umode (e)
{
    if ("user" in e && e.user)
    {
        e.user.updateHeader();
        this.display(getMsg(MSG_USER_MODE, [e.user.unicodeName, e.params[2]]),
                     MT_MODE);
    }
    else
    {
        this.display(getMsg(MSG_USER_MODE, [e.params[1], e.params[2]]),
                     MT_MODE);
    }
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

CIRCNetwork.prototype.reclaimName =
function my_reclaimname()
{
    var network = this;

    function callback() {
        network.reclaimName();
    };

    if ("pendingReclaimCheck" in this)
        delete this.pendingReclaimCheck;

    // Function to attempt to get back the nickname the user wants.
    if ((this.state != NET_ONLINE) || !this.primServ)
        return false;

    if (this.primServ.me.unicodeName == this.prefs["nickname"])
        return false;

    this.reclaimLeft -= this.RECLAIM_WAIT;

    if (this.reclaimLeft <= 0)
        return false;

    this.pendingReclaimCheck = true;
    this.primServ.sendData("NICK " + fromUnicode(this.prefs["nickname"], this) + "\n");

    setTimeout(callback, this.RECLAIM_WAIT);

    return true;
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

    client.munger.getRule(".mailto").enabled = client.prefs["munger.mailto"];
    this.display (msg, "PRIVMSG", e.user, this);
    client.munger.getRule(".mailto").enabled = false;
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

    // Update conference mode now we have a complete user list.
    this._updateConferenceMode();
}

CIRCChannel.prototype.onTopic = /* user changed topic */
CIRCChannel.prototype.on332 = /* TOPIC reply */
function my_topic (e)
{
    client.munger.getRule(".mailto").enabled = client.prefs["munger.mailto"];
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
    client.munger.getRule(".mailto").enabled = false;
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
function my_bans(e)
{
    if ("pendingBanList" in this)
        return;

    var msg = getMsg(MSG_BANLIST_ITEM,
                     [e.user.unicodeName, e.ban, this.unicodeName, e.banTime]);
    if (this.iAmHalfOp() || this.iAmOp())
        msg += " " + getMsg(MSG_BANLIST_BUTTON, "mode -b " + e.ban);

    client.munger.getRule(".inline-buttons").enabled = true;
    this.display(msg, "BAN");
    client.munger.getRule(".inline-buttons").enabled = false;
}

CIRCChannel.prototype.on368 =
function my_endofbans(e)
{
    if ("pendingBanList" in this)
        return;

    this.display(getMsg(MSG_BANLIST_END, this.unicodeName), "BAN");
}

CIRCChannel.prototype.on348 = /* channel except stuff */
function my_excepts(e)
{
    if ("pendingExceptList" in this)
        return;

    var msg = getMsg(MSG_EXCEPTLIST_ITEM, [e.user.unicodeName, e.except,
                                           this.unicodeName, e.exceptTime]);
    if (this.iAmHalfOp() || this.iAmOp())
        msg += " " + getMsg(MSG_EXCEPTLIST_BUTTON, "mode -e " + e.except);

    client.munger.getRule(".inline-buttons").enabled = true;
    this.display(msg, "EXCEPT");
    client.munger.getRule(".inline-buttons").enabled = false;
}

CIRCChannel.prototype.on349 =
function my_endofexcepts(e)
{
    if ("pendingExceptList" in this)
        return;

    this.display(getMsg(MSG_EXCEPTLIST_END, this.unicodeName), "EXCEPT");
}

CIRCChannel.prototype.on482 =
function my_needops(e)
{
    if ("pendingExceptList" in this)
        return;

    this.display(getMsg(MSG_CHANNEL_NEEDOPS, this.unicodeName), MT_ERROR);
}

CIRCChannel.prototype.onNotice =
function my_notice (e)
{
    client.munger.getRule(".mailto").enabled = client.prefs["munger.mailto"];
    this.display(e.decodeParam(2), "NOTICE", e.user, this);
    client.munger.getRule(".mailto").enabled = false;
}

CIRCChannel.prototype.onCTCPAction =
function my_caction (e)
{
    client.munger.getRule(".mailto").enabled = client.prefs["munger.mailto"];
    this.display (e.CTCPData, "ACTION", e.user, this);
    client.munger.getRule(".mailto").enabled = false;
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
        var params = [e.user.unicodeName, e.channel.unicodeName];
        this.display(getMsg(MSG_YOU_JOINED, params), "JOIN",
                     e.server.me, this);
        /* Tell the user that conference mode is on, lest they forget (if it
         * subsequently turns itself off, they'll get a message anyway).
         */
        if (this.prefs["conference.enabled"])
            this.display(MSG_CONF_MODE_STAYON);
        if (client.globalHistory)
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
        if (!this.prefs["conference.enabled"])
        {
            this.display(getMsg(MSG_SOMEONE_JOINED,
                                [e.user.unicodeName, e.user.name, e.user.host,
                                 e.channel.unicodeName]),
                         "JOIN", e.user, this);
        }

        /* Only do this for non-me joins so us joining doesn't reset it (when
         * we join the usercount is always 1). Also, do this after displaying
         * the join message so we don't get cryptic effects such as a user
         * joining causes *only* a "Conference mode enabled" message.
         */
        this._updateConferenceMode();
    }

    this._addUserToGraph(e.user);
    /* We don't want to add ourself here, since the names reply we'll be
     * getting right after the join will include us as well! (FIXME)
     */
    if (!userIsMe(e.user))
        this.addUsers([e.user]);
    if (client.currentObject == this)
        updateUserList();
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
        var params = [e.user.unicodeName, e.channel.unicodeName];
        this.display (getMsg(MSG_YOU_LEFT, params), "PART", e.user, this);

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

        if (this.deleteWhenDone)
            this.dispatch("delete-view");

        delete this.deleteWhenDone;
    }
    else
    {
        /* We're ok to update this before the message, because the only thing
         * that can happen is *disabling* of conference mode.
         */
        this._updateConferenceMode();

        if (!this.prefs["conference.enabled"])
        {
            var msg = MSG_SOMEONE_LEFT;
            if (e.reason)
                msg = MSG_SOMEONE_LEFT_REASON;

            this.display(getMsg(msg, [e.user.unicodeName, e.channel.unicodeName,
                                      e.reason]),
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
                                 [e.lamer.unicodeName, e.channel.unicodeName,
                                  e.user.unicodeName, e.reason]),
                          "KICK", e.user, this);
        }
        else
        {
            this.display (getMsg(MSG_YOURE_GONE,
                                 [e.lamer.unicodeName, e.channel.unicodeName,
                                  MSG_SERVER, e.reason]),
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

        this.display(getMsg(MSG_SOMEONE_GONE,
                            [e.lamer.unicodeName, e.channel.unicodeName,
                             enforcerProper, e.reason]),
                     "KICK", e.user, this);
    }

    this._removeUserFromGraph(e.lamer);
    this.removeUsers([e.lamer]);
    this.updateHeader();
}

CIRCChannel.prototype.onChanMode =
function my_cmode (e)
{
    if (e.code == "MODE")
    {
        var msg = e.decodeParam(1);
        for (var i = 2; i < e.params.length; i++)
            msg += " " + e.decodeParam(i);

        var source = e.user ? e.user.unicodeName : e.source;
        this.display(getMsg(MSG_MODE_CHANGED, [msg, source]),
                     "MODE", (e.user || null), this);
    }
    else if ("pendingModeReply" in this)
    {
        var msg = e.decodeParam(3);
        for (var i = 4; i < e.params.length; i++)
            msg += " " + e.decodeParam(i);

        var view = ("messages" in this && this.messages) ? this : e.network;
        view.display(getMsg(MSG_MODE_ALL, [this.unicodeName, msg]), "MODE");
        delete this.pendingModeReply;
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
    else if (!this.prefs["conference.enabled"])
    {
        this.display(getMsg(MSG_NEWNICK_NOTYOU, [e.oldNick,
                                                 e.user.unicodeName]),
                     "NICK", e.user, this);
    }

    e.user.updateGraphResource();
    this.updateUsers([e.user]);
    if (client.currentObject == this)
        updateUserList();
}

CIRCChannel.prototype.onQuit =
function my_cquit (e)
{
    if (userIsMe(e.user))
    {
        /* I dont think this can happen */
        var pms = [e.user.unicodeName, e.server.parent.unicodeName, e.reason];
        this.display (getMsg(MSG_YOU_QUIT, pms),"QUIT", e.user, this);
    }
    else
    {
        // See onPart for why this is ok before the message.
        this._updateConferenceMode();

        if (!this.prefs["conference.enabled"])
        {
            this.display(getMsg(MSG_SOMEONE_QUIT,
                                [e.user.unicodeName,
                                 e.server.parent.unicodeName, e.reason]),
                         "QUIT", e.user, this);
        }
    }

    this._removeUserFromGraph(e.user);
    this.removeUsers([e.user]);
    this.updateHeader();
}

CIRCUser.prototype.onInit =
function user_oninit ()
{
    this.logFile = null;
    this.lastShownAwayMessage = "";
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

    client.munger.getRule(".mailto").enabled = client.prefs["munger.mailto"];
    this.display(e.decodeParam(2), "PRIVMSG", e.user, e.server.me);
    client.munger.getRule(".mailto").enabled = false;
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
    var msg = e.decodeParam(2);
    var displayMailto = client.prefs["munger.mailto"];

    var ary = msg.match(/^\[(\S+)\]\s+/);
    if (ary)
    {
        var channel = e.server.getChannel(ary[1]);
        if (channel)
        {
            client.munger.getRule(".mailto").enabled = displayMailto;
            channel.display(msg, "NOTICE", this, e.server.me);
            client.munger.getRule(".mailto").enabled = false;
            return;
        }
    }

    client.munger.getRule(".mailto").enabled = displayMailto;
    this.display(msg, "NOTICE", this, e.server.me);
    client.munger.getRule(".mailto").enabled = false;
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

    client.munger.getRule(".mailto").enabled = client.prefs["munger.mailto"];
    this.display(e.CTCPData, "ACTION", this, e.server.me);
    client.munger.getRule(".mailto").enabled = false;
}

CIRCUser.prototype.onUnknownCTCP =
function my_unkctcp (e)
{
    this.parent.parent.display (getMsg(MSG_UNKNOWN_CTCP,
                                       [e.CTCPCode, e.CTCPData,
                                        e.user.unicodeName]),
                                "BAD-CTCP", this, e.server.me);
}

function onDCCAutoAcceptTimeout(o, folder)
{
    // user may have already accepted or declined
    if (o.state.state != DCC_STATE_REQUESTED)
        return;

    if (o.TYPE == "IRCDCCChat")
    {
        o.accept();
        display(getMsg(MSG_DCCCHAT_ACCEPTED, o._getParams()), "DCC-CHAT");
    }
    else
    {
        var dest, leaf, tries = 0;
        while (true)
        {
            leaf = escapeFileName(o.filename);
            if (++tries > 1)
            {
                // A file with the same name as the offered file already exists
                // in the user's download folder. Add [x] before the extension.
                // The extension is the last dot to the end of the string,
                // unless it is one of the special-cased compression extensions,
                // in which case the second to last dot is used. The second
                // extension can only contain letters, to avoid mistakes like
                // "patch-version1[2].0.gz". If no file extension is present,
                // the [x] is just appended to the filename.
                leaf = leaf.replace(/(\.[a-z]*\.(gz|bz2|z)|\.[^\.]*|)$/i,
                                    "[" + tries + "]$&");
            }

            dest = getFileFromURLSpec(folder);
            dest.append(leaf);
            if (!dest.exists())
                break;
        }
        o.accept(dest);
        display(getMsg(MSG_DCCFILE_ACCEPTED, o._getParams()), "DCC-FILE");
    }
}

CIRCUser.prototype.onDCCChat =
function my_dccchat(e)
{
    if (!jsenv.HAS_SERVER_SOCKETS || !client.prefs["dcc.enabled"])
        return;

    var u = client.dcc.addUser(e.user, e.host);
    var c = client.dcc.addChat(u, e.port);

    var str = MSG_DCCCHAT_GOT_REQUEST;
    var cmds = getMsg(MSG_DCC_COMMAND_ACCEPT, "dcc-accept " + c.id) + " " +
               getMsg(MSG_DCC_COMMAND_DECLINE, "dcc-decline " + c.id);

    var allowList = this.parent.parent.prefs["dcc.autoAccept.list"];
    for (var m = 0; m < allowList.length; ++m)
    {
        if (hostmaskMatches(e.user, getHostmaskParts(allowList[m])))
        {
            var acceptDelay = client.prefs["dcc.autoAccept.delay"];
            if (acceptDelay == 0)
            {
                str = MSG_DCCCHAT_ACCEPTING_NOW;
            }
            else
            {
                str = MSG_DCCCHAT_ACCEPTING;
                cmds = [(acceptDelay / 1000), cmds];
            }
            setTimeout(onDCCAutoAcceptTimeout, acceptDelay, c);
            break;
        }
    }

    client.munger.getRule(".inline-buttons").enabled = true;
    this.parent.parent.display(getMsg(str, c._getParams().concat(cmds)),
                               "DCC-CHAT");
    client.munger.getRule(".inline-buttons").enabled = false;

    // Pass the event over to the DCC Chat object.
    e.set = "dcc-chat";
    e.destObject = c;
    e.destMethod = "onGotRequest";
}

CIRCUser.prototype.onDCCSend =
function my_dccsend(e)
{
    if (!jsenv.HAS_SERVER_SOCKETS || !client.prefs["dcc.enabled"])
        return;

    var u = client.dcc.addUser(e.user, e.host);
    var f = client.dcc.addFileTransfer(u, e.port, e.file, e.size);

    var str = MSG_DCCFILE_GOT_REQUEST;
    var cmds = getMsg(MSG_DCC_COMMAND_ACCEPT, "dcc-accept " + f.id) + " " +
               getMsg(MSG_DCC_COMMAND_DECLINE, "dcc-decline " + f.id);

    var allowList = this.parent.parent.prefs["dcc.autoAccept.list"];
    for (var m = 0; m < allowList.length; ++m)
    {
        if (hostmaskMatches(e.user, getHostmaskParts(allowList[m]),
                            this.parent))
        {
            var acceptDelay = client.prefs["dcc.autoAccept.delay"];
            if (acceptDelay == 0)
            {
                str = MSG_DCCFILE_ACCEPTING_NOW;
            }
            else
            {
                str = MSG_DCCFILE_ACCEPTING;
                cmds = [(acceptDelay / 1000), cmds];
            }
            setTimeout(onDCCAutoAcceptTimeout, acceptDelay,
                       f, this.parent.parent.prefs["dcc.downloadsFolder"]);
            break;
        }
    }

    client.munger.getRule(".inline-buttons").enabled = true;
    this.parent.parent.display(getMsg(str,[e.user.unicodeName,
                                           e.host, e.port, e.file,
                                           getSISize(e.size)].concat(cmds)),
                               "DCC-FILE");
    client.munger.getRule(".inline-buttons").enabled = false;

    // Pass the event over to the DCC File object.
    e.set = "dcc-file";
    e.destObject = f;
    e.destMethod = "onGotRequest";
}

CIRCUser.prototype.onDCCReject =
function my_dccreject(e)
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
}

CIRCDCCChat.prototype._getParams =
function my_dccgetparams()
{
    return [this.user.unicodeName, this.localIP, this.port];
}

CIRCDCCChat.prototype.onPrivmsg =
function my_dccprivmsg(e)
{
    client.munger.getRule(".mailto").enabled = client.prefs["munger.mailto"];
    this.displayHere(toUnicode(e.line, this), "PRIVMSG", e.user, "ME!");
    client.munger.getRule(".mailto").enabled = false;
}

CIRCDCCChat.prototype.onCTCPAction =
function my_uaction(e)
{
    client.munger.getRule(".mailto").enabled = client.prefs["munger.mailto"];
    this.displayHere(e.CTCPData, "ACTION", e.user, "ME!");
    client.munger.getRule(".mailto").enabled = false;
}

CIRCDCCChat.prototype.onUnknownCTCP =
function my_unkctcp(e)
{
    this.displayHere(getMsg(MSG_UNKNOWN_CTCP, [e.CTCPCode, e.CTCPData,
                                               e.user.unicodeName]),
                     "BAD-CTCP", e.user, "ME!");
}

CIRCDCCChat.prototype.onConnect =
function my_dccconnect(e)
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
            this.localIP/*FIXME*/, this.port/*FIXME?*/];
}

CIRCDCCFileTransfer.prototype.onConnect =
function my_dccfileconnect(e)
{
    this.displayHere(getMsg(MSG_DCCFILE_OPENED, this._getParams()), "DCC-FILE");
    this.busy = true;
    this.progress = 0;
    this.speed = 0;
    updateProgress();
    this._lastUpdate = new Date();
    this._lastPosition = 0;
    this._lastSpeedTime = new Date();
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
        updateTitle();

        var tab = getTabForObject(this);
        if (tab)
            tab.setAttribute("label", this.viewName + " (" + pcent + "%)");

        var change = (this.position - this._lastPosition);
        var speed = change / ((now - this._lastSpeedTime) / 1000); // B/s
        this._lastSpeedTime = now;

        /* Use an average of the last speed, and this speed, so we get a little
         * smoothing to it.
         */
        this.speed = (this.speed + speed) / 2;
        this.updateHeader();
        this._lastPosition = this.position;
    }

    // If it's also been 10s or more since we last displayed a msg...
    if (now - this._lastUpdate > 10000)
    {
        this.displayHere(getMsg(MSG_DCCFILE_PROGRESS,
                                [pcent, getSISize(this.position),
                                 getSISize(this.size), getSISpeed(this.speed)]),
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

    var msg, tab = getTabForObject(this);
    if (tab)
        tab.setAttribute("label", this.viewName + " (DONE)");

    if (this.state.dir == DCC_DIR_GETTING)
    {
        msg = getMsg(MSG_DCCFILE_CLOSED_SAVED,
                     this._getParams().concat(this.localPath));
    }
    else
    {
        msg = getMsg(MSG_DCCFILE_CLOSED_SENT, this._getParams());
    }
    this.display(msg, "DCC-FILE");
}

var CopyPasteHandler = new Object();

CopyPasteHandler.allowDrop =
CopyPasteHandler.allowStartDrag =
CopyPasteHandler.onCopyOrDrag =
function phand_bogus()
{
    return true;
}

CopyPasteHandler.onPasteOrDrop =
function phand_onpaste(e, data)
{
    // XXXbug 329487: The effect of onPasteOrDrop's return value is actually the
    //                exact opposite of the definition in the IDL.

    // Don't mess with the multiline box at all.
    if (client.prefs["multiline"])
        return true;

    var str = new Object();
    var strlen = new Object();
    data.getTransferData("text/unicode", str, strlen);
    str.value.QueryInterface(Components.interfaces.nsISupportsString);
    str.value.data = str.value.data.replace(/(^\s*[\r\n]+|[\r\n]+\s*$)/g, "");

    // XXX part of what follows is a very ugly hack to make links (with a title)
    // not open the multiline box. We 'should' be able to ask the transferable
    // what flavours it supports, but testing showed that by the time we can ask
    // for that info, it's forgotten about everything apart from text/unicode.
    var lines = str.value.data.split("\n");
    var m = lines[0].match(client.linkRE);

    if ((str.value.data.indexOf("\n") == -1) ||
        (m && (m[0] == lines[0]) && (lines.length == 2)))
    {
        // If, after stripping leading/trailing empty lines, the string is a
        // single line, or it's a link with a title, put it back in
        // the transferable and return.
        data.setTransferData("text/unicode", str.value,
                             str.value.data.length * 2);
        return true;
    }

    // If it's a drop, move the text cursor to the mouse position.
    if (e && ("rangeOffset" in e))
        client.input.setSelectionRange(e.rangeOffset, e.rangeOffset);

    str = client.input.value.substr(0, client.input.selectionStart) +
          str.value.data + client.input.value.substr(client.input.selectionEnd);
    client.prefs["multiline"] = true;
    client.input.value = str;
    return false;
}

CopyPasteHandler.QueryInterface =
function phand_qi(iid)
{
    if (iid.equals(Components.interfaces.nsISupports) ||
        iid.equals(Components.interfaces.nsIClipboardDragDropHooks))
        return this;

    throw Components.results.NS_ERROR_NO_INTERFACE;
}
