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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Robert Ginda, <rginda@netscape.com>, original author
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

var initialized = false;

var view;
var client;
var mainWindow;
var clickHandler;

var dd;
var getMsg;
var getObjectDetails;

var header = null;
var headers = {
    IRCClient: {
        prefix: "cli-",
        fields: ["container", "netcount", "version-container", "version",
                 "connectcount"],
        update: updateClient
    },

    IRCNetwork: {
        prefix: "net-",
        fields: ["container", "url-anchor", "status", "lag"],
        update: updateNetwork
    },

    IRCChannel: {
        prefix: "ch-",
        fields: ["container", "url-anchor", "modestr", "usercount",
                 "topicnodes", "topicinput"],
        update: updateChannel
    },

    IRCUser: {
        prefix: "usr-",
        fields: ["container", "url-anchor", "serverstr", "title",
                 "descnodes"],
        update: updateUser
    },
    
    IRCDCCChat: {
        prefix: "dcc-chat-",
        fields: ["container", "url-anchor", "remotestr", "title"],
        update: updateDCCChat
    }
};

var initOutputWindow = stock_initOutputWindow;

function stock_initOutputWindow(newClient, newView, newClickHandler)
{
    function initHeader()
    {
        /* it's better if we wait a half a second before poking at these
         * dom nodes. */
        setHeaderState(view.prefs["displayHeader"]);
        updateHeader();
        var div = document.getElementById("messages-outer");
        div.removeAttribute("hidden");
        window.scrollTo(0, window.document.height);
    };

    client = newClient;
    view = newView;
    clickHandler = newClickHandler;
    mainWindow = client.mainWindow;
    
    client.messageManager.importBundle(client.defaultBundle, window);

    getMsg = mainWindow.getMsg;
    getObjectDetails = mainWindow.getObjectDetails;
    dd = mainWindow.dd;

    changeCSS(view.prefs["motif.current"]);
    
    var output = document.getElementById("output");
    output.appendChild(view.messages);

    if (view.TYPE in headers)
    {
        header = cacheNodes(headers[view.TYPE].prefix,
                            headers[view.TYPE].fields);
        header.update = headers[view.TYPE].update;
    }

    var splash = document.getElementById("splash");
    var name;
    if ("unicodeName" in view)
        name = view.unicodeName;
    else if ("properNick" in view)
        name = view.properNick;
    else
        name = view.name;
    splash.appendChild(document.createTextNode(name));

    setTimeout(initHeader, 500);

    initialized = true;
}

function onTopicNodesClick(e)
{
    if (!clickHandler(e))
    {
        if (e.which != 1)
            return;

        startTopicEdit();
    }

    e.stopPropagation();
}

function onTopicKeypress(e)
{
    switch (e.keyCode)
    {
        case 13: /* enter */
            var topic = header["topicinput"].value;
            topic = mainWindow.replaceColorCodes(topic);
            view.setTopic(topic);
            view.dispatch("focus-input");
            break;
            
        case 27: /* esc */
            view.dispatch("focus-input");
            break;
            
        default:
            client.mainWindow.onInputKeypress(header["topicinput"]);
    }
}

function startTopicEdit()
{
    var me = view.getUser(view.parent.me.nick);
    if (!me || (!view.mode.publicTopic && !me.isOp && !me.isHalfOp) ||
        !header["topicinput"].hasAttribute("hidden"))
    {
        return;
    }
    
    header["topicinput"].value = mainWindow.decodeColorCodes(view.topic);

    header["topicnodes"].setAttribute("hidden", "true")
    header["topicinput"].removeAttribute("hidden");
    header["topicinput"].focus();
    header["topicinput"].selectionStart = 0;
}

function cancelTopicEdit()
{
    if (!header["topicnodes"].hasAttribute("hidden"))
        return;
    
    header["topicinput"].setAttribute("hidden", "true")
    header["topicnodes"].removeAttribute("hidden");
}

function cacheNodes(pfx, ary, nodes)
{
    if (!nodes)
        nodes = new Object();

    for (var i = 0; i < ary.length; ++i)
        nodes[ary[i]] = document.getElementById(pfx + ary[i]);

    return nodes;
}

function changeCSS(url, id)
{
    if (!id)
        id = "main-css";
    
    node = document.getElementById(id);

    if (!node)
    {
        node = document.createElement("link");
        node.setAttribute("id", id);
        node.setAttribute("rel", "stylesheet");
        node.setAttribute("type", "text/css");
        var head = document.getElementsByTagName("head")[0];
        head.appendChild(node);
    }
    else
    {
        if (node.getAttribute("href") == url)
            return;
    }

    node.setAttribute("href", url);
    window.scrollTo(0, window.document.height);
}

function setText(field, text, checkCondition)
{
    if (!header[field].firstChild)
        header[field].appendChild(document.createTextNode(""));

    if (typeof text != "string")
    {
        text = MSG_UNKNOWN;
        if (checkCondition)
            setAttribute(field, "condition", "red");
    }
    else if (checkCondition)
    {
        setAttribute(field, "condition", "green");
    }
                
    header[field].firstChild.data = text;
}

function setAttribute(field, name, value)
{
    if (!value)
        value = "true";

    header[field].setAttribute(name, value);
}

function removeAttribute(field, name)
{
    header[field].removeAttribute(name);
}

function hasAttribute(field, name)
{
    header[field].hasAttribute(name);
}

function setHeaderState(state)
{
    if (header)
    {
        if (state)
        {
            updateHeader();
            removeAttribute("container", "hidden");
        }
        else
        {
            setAttribute("container", "hidden");
        }
    }
}

function updateHeader()
{
    if (!header || hasAttribute("container", "hidden"))
        return;

    for (var id in header)
    {
        var value;

        if (id == "url-anchor")
        {
            value = view.getURL();
            setAttribute("url-anchor", "href", value);
            setText("url-anchor", value);
        }
        else if (id in view)
        {
            setText(id, view[id]);
        }
    }

    if (header.update)
        header.update();
}

function updateClient()
{
    var n = 0, c = 0;
    for (name in client.networks)
    {
        ++n;
        if (client.networks[name].isConnected())
            ++c;
    }

    setAttribute("version-container", "title", client.userAgent);
    setAttribute("version-container", "condition", mainWindow.__cz_condition);
    setText("version", mainWindow.__cz_version);
    setText("netcount", String(n));
    setText("connectcount", String(c));
}

function updateNetwork()
{
    if (view.connecting)
    {
        setText("status", MSG_CONNECTING);
        setAttribute("status","condition", "yellow");
        removeAttribute("status", "title");
        setText("lag", MSG_UNKNOWN);
    }
    else if (view.isConnected())
    {
        setText("status", MSG_CONNECTED);
        setAttribute("status","condition", "green");
        setAttribute("status", "title",
                     getMsg(MSG_CONNECT_VIA, view.primServ.name));
        if (view.primServ.lag != -1)
            setText("lag", getMsg(MSG_FMT_SECONDS, view.primServ.lag));
        else
            setText("lag", MSG_UNKNOWN);
        
    }
    else
    {
        setText("status", MSG_DISCONNECTED);
        setAttribute("status","condition", "red");
        removeAttribute("status", "title");
        setText("lag", MSG_UNKNOWN);
    }
}

function updateChannel()
{
    header["topicnodes"].removeChild(header["topicnodes"].firstChild);

    if (view.active)
    {
        var str = view.mode.getModeStr();
        if (!str)
            str = MSG_NO_MODE;
        setText("modestr", str);
        setAttribute("modestr", "condition", "green");

        setText("usercount", getMsg(MSG_FMT_USERCOUNT,
                                    [view.getUsersLength(), view.opCount,
                                     view.halfopCount, view.voiceCount]));
        setAttribute("usercount", "condition", "green");

        if (view.topic)
        {
            var nodes = client.munger.munge(view.topic, null,
                                            getObjectDetails(view));
            header["topicnodes"].appendChild(nodes);
        }
        else
        {
            setText("topicnodes", MSG_NONE);
        }
    }
    else
    {
        setText("modestr", MSG_UNKNOWN);
        setAttribute("modestr", "condition", "red");
        setText("usercount", MSG_UNKNOWN);
        setAttribute("usercount", "condition", "red");
        setText("topicnodes", MSG_UNKNOWN);
    }

}

function updateUser()
{
    var source;
    if (view.name)
        source = "<" + view.name + "@" + view.host + ">";
    else
        source = MSG_UNKNOWN;

    if (view.parent.isConnected)
        setText("serverstr", view.connectionHost, true);
    else
        setText("serverstr", null, true);

    setText("title", getMsg(MSG_TITLE_USER, [view.properNick, source]));

    header["descnodes"].removeChild(header["descnodes"].firstChild);
    if (typeof view.desc != "undefined")
    {
        var nodes = client.munger.munge(view.desc, null,
                                        getObjectDetails(view));
            header["descnodes"].appendChild(nodes);
    }
    else
    {
        setText("descnodes", "");
    }
}

function updateDCCChat()
{
    var source;
    if (view.user)
        source = view.user.displayName;
    else
        source = MSG_UNKNOWN;

    if (view.state == 3)
        setText("remotestr", view.remoteIP + ":" + view.port, true);
    else
        setText("remotestr", null, true);

    setText("title", getMsg(MSG_TITLE_USER, [view.user.displayName, source]));
}
