/* ***** BEGIN LICENSE BLOCK *****
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
 * The Initial Developer of the Original Code is James Ross.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   James Ross, <silver@warwickcompsoc.co.uk>
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

var client;
var network;
var channels = new Array();
var createChannelItem;

var channelTreeShare = new Object();
var channelTreeView;

const colIDToSortKey = { chanColName: "name",
                         chanColUsers: "users",
                         chanColTopic: "topic" };
const sortKeyToColID = { name: "chanColName",
                         users: "chanColUsers",
                         topic: "chanColTopic" };

function onLoad()
{
    function ondblclick(event) { channelTreeView.onRouteDblClick(event); };
    function onkeypress(event) { channelTreeView.onRouteKeyPress(event); };
    function onfocus(event)    { channelTreeView.onRouteFocus(event); };
    function onblur(event)     { channelTreeView.onRouteBlur(event); };

    function doJoin()
    {
        if (joinChannel())
            window.close();
    };

    client = window.arguments[0].client;

    window.toUnicode = client.mainWindow.toUnicode;
    window.getMsg = client.mainWindow.getMsg;
    window.MSG_CHANNEL_OPENED = client.mainWindow.MSG_CHANNEL_OPENED;
    window.MT_INFO = client.mainWindow.MT_INFO;

    // Import "MSG_CD_*"...
    for (var m in client.mainWindow)
    {
        if (m.substr(0, 7) == "MSG_CD_")
            window[m] = client.mainWindow[m];
    }

    network = client.mainWindow.getObjectDetails(client.currentObject).network;

    var tree = document.getElementById("channelList");

    channelTreeView = new XULTreeView(channelTreeShare);
    channelTreeView.onRowCommand = doJoin;
    channelTreeView.cycleHeader = changeSort;
    tree.treeBoxObject.view = channelTreeView;

    // Sort by user count, decending.
    changeSort("chanColUsers");

    tree.addEventListener("dblclick", ondblclick, false);
    tree.addEventListener("keypress", onkeypress, false);
    tree.addEventListener("focus", onfocus, false);
    tree.addEventListener("blur", onblur, false);

    createChannelItem = new ChannelEntry("", "", MSG_CD_CREATE);
    createChannelItem.first = true;
    channelTreeView.childData.appendChild(createChannelItem);

    document.title = getMsg(MSG_CD_TITLE, [network.unicodeName,
                                           network.getURL()]);

    setTimeout(startListRequest, 100, false);
}

function onKeyPress(event)
{
    if (event.keyCode == event.DOM_VK_UP)
    {
        if (channelTreeView.selectedIndex > 0)
            channelTreeView.selectedIndex = channelTreeView.selectedIndex - 1;
        event.preventDefault();
    }
    else if (event.keyCode == event.DOM_VK_DOWN)
    {
        if (channelTreeView.selectedIndex < channelTreeView.rowCount - 1)
            channelTreeView.selectedIndex = channelTreeView.selectedIndex + 1;
        event.preventDefault();
    }
}

function onSelectionChange()
{
    var joinBtn = document.getElementById("joinBtn");
    joinBtn.disabled = (channelTreeView.selectedIndex == -1);
}

function runFilter()
{
    function process()
    {
        var exactMatch = null;
        var channelText = text;
        if (!channelText.match(/^[#&+!]/))
            channelText = "#" + channelText;

        for (var i = 0; i < channels.length; i++)
        {
            var match = (channels[i].name.toLowerCase().indexOf(text) != -1) ||
                        (searchTopics && 
                         (channels[i].topic.toLowerCase().indexOf(text) != -1));

            if (minUsers && (channels[i].users < minUsers))
                match = false;
            if (maxUsers && (channels[i].users > maxUsers))
                match = false;

            if (channels[i].isHidden)
            {
                if (match)
                    channels[i].unHide();
            }
            else
            {
                if (!match)
                    channels[i].hide();
            }

            if (match && (channels[i].name.toLowerCase() == channelText))
                exactMatch = channels[i];

            if ((i % 100) == 0)
                updateProgress(MSG_CD_FILTERING, 100 * i / channels.length);
        }
        updateProgress();

        createChannelItem.name = channelText;
        var tree = document.getElementById("channelList");
        tree.treeBoxObject.invalidateRow(0);

        if (exactMatch)
        {
            if (!createChannelItem.isHidden)
                createChannelItem.hide();
            channelTreeView.selectedIndex = exactMatch.calculateVisualRow();
        }
        else
        {
            if (createChannelItem.isHidden)
                createChannelItem.unHide();
            if (channelTreeView.selectedIndex == -1)
            {
                // The special 'create' row is visible, so prefer the first
                // real channel over it.
                if (channelTreeView.rowCount >= 2)
                    channelTreeView.selectedIndex = 1;
                else if (channelTreeView.rowCount >= 1)
                    channelTreeView.selectedIndex = 0;
            }
        }

    };

    var text = document.getElementById("filterText").value.toLowerCase();
    var searchTopics = document.getElementById("searchTopics").checked;
    var minUsers = document.getElementById("minUsers").value * 1;
    var maxUsers = document.getElementById("maxUsers").value * 1;

    if (channels.length > 1000)
        updateProgress(getMsg(MSG_CD_FILTERING1, channels.length));
    else
        updateProgress(getMsg(MSG_CD_FILTERING2, channels.length));

    setTimeout(process, 100);
}

function joinChannel()
{
    var index = channelTreeView.selectedIndex;
    if (index == -1)
        return false;

    var row = channelTreeView.childData.locateChildByVisualRow(index);
    network.dispatch("join", { channelName: row.name });

    return true;
}

function focusSearch()
{
    document.getElementById("filterText").focus();
}

function refreshList()
{
    startListRequest(true);
}

function updateProgress(label, pro)
{
    if (label)
    {
        document.getElementById("loadLabel").value = label;
    }
    else
    {
        var msg = getMsg(MSG_CD_SHOWING,
             [(channelTreeView.rowCount - (createChannelItem.isHidden ? 0 : 1)),
              channels.length]);
        document.getElementById("loadLabel").value = msg;
    }

    var loadBarDeckIndex = ((typeof pro == "undefined") ? 1 : 0);
    document.getElementById("loadBarDeck").selectedIndex = loadBarDeckIndex;

    if ((typeof pro == "undefined") || (pro == -1))
    {
        document.getElementById("loadBar").mode = "undetermined";
    }
    else
    {
        document.getElementById("loadBar").mode = "determined";
        document.getElementById("loadBar").position = pro;
    }
}

function changeSort(col)
{
    if (typeof col == "object")
        col = col.id;

    col = colIDToSortKey[col];
    // Users default to decending, others accending.
    var dir = (col == "users" ? -1 : 1);

    if (col == channelTreeShare.sortColumn)
        dir = -channelTreeShare.sortDirection;

    var colID = sortKeyToColID[channelTreeShare.sortColumn];
    var colNode = document.getElementById(colID);
    if (colNode)
    {
        colNode.removeAttribute("sortActive");
        colNode.removeAttribute("sortDirection");
    }

    channelTreeView.childData.setSortColumn(col, dir);

    colID = sortKeyToColID[channelTreeShare.sortColumn];
    colNode = document.getElementById(colID);
    if (colNode)
    {
        colNode.setAttribute("sortActive", "true");
        var sortDir = (dir > 0 ? "ascending" : "descending");
        colNode.setAttribute("sortDirection", sortDir);
    }
}

function startListRequest(force)
{
    if (("_list" in network) && !network._list.done)
    {
        updateProgress(MSG_CD_WAIT_LIST);
        setTimeout(startListRequest, 1000, force);
        return;
    }

    updateProgress(MSG_CD_FETCHING, -1);
    document.getElementById("refreshNow").disabled = true;

    var file = new LocalFile(network.prefs["logFileName"]);
    file.localFile.leafName = "list.txt";

    if (file.localFile.exists() && !force)
    {
        startListFileLoad();
        return;
    }

    network.list("", file.localFile.path);

    setTimeout(duringListRequest, 1000);
}

function duringListRequest()
{
    var lbl = document.getElementById("loadLabel");
    updateProgress(getMsg(MSG_CD_FETCHED, network._list.count), -1);

    if (!network._list.done)
        setTimeout(duringListRequest, 1000);
    else
        afterListRequest();
}

function afterListRequest()
{
    if ("error" in network._list)
    {
        updateProgress(MSG_CD_ERROR_LIST);
        document.getElementById("refreshNow").disabled = false;
        // Can't seem to "undefine" the parameters using the function format.
        setTimeout("updateProgress()", 10000);
    }
    else
    {
        updateProgress();
        startListFileLoad();
    }
}

var pendingData;

function startListFileLoad()
{
    // Nuke contents.
    channelTreeView.selectedIndex = -1;
    // Should ideally be calling freeze()/thaw() here, but unfortunately they
    // mess up the selection, even if they make this so much faster.
    while (channelTreeView.childData.childData.length > 1)
        channelTreeView.childData.removeChildAtIndex(1);

    // Nuke more stuff.
    channels = new Array();
    pendingData = "";

    // And... here we go.
    var file = new LocalFile(network.prefs["logFileName"]);
    file.localFile.leafName = "list.txt";
    file = new LocalFile(file.localFile, "<");

    duringListFileLoad(file);
}

function duringListFileLoad(file)
{
    var data = file.read(10000);
    pendingData += data;

    while (pendingData.indexOf("\n") != -1)
    {
        var lines = pendingData.split("\n");
        pendingData = lines.pop();

        for (var i = 0; i < lines.length; i++)
        {
            var ary = lines[i].match(/^([^ ]+) ([^ ]+) (.*)$/);
            if (ary)
            {
                var chan = new ChannelEntry(toUnicode(ary[1], "UTF-8"), ary[2],
                                            toUnicode(ary[3], "UTF-8"));
                channels.push(chan);
            }
        }
    }
    updateProgress(getMsg(MSG_CD_LOADED, channels.length), -1);

    if (file.inputStream.available() != 0)
        setTimeout(duringListFileLoad, 100, file);
    else
        afterListFileLoad(file);
}

function afterListFileLoad(file)
{
    document.getElementById("refreshNow").disabled = false;
    if (channels.length > 0)
        channelTreeView.childData.appendChildren(channels);
    updateProgress();
    runFilter();
}

// Tree ChannelEntry objects //
function ChannelEntry(name, users, topic)
{
    this.setColumnPropertyName("chanColName", "name");
    this.setColumnPropertyName("chanColUsers", "users");
    this.setColumnPropertyName("chanColTopic", "topic");

    // Nuke color codes and bold etc.
    topic = topic.replace(/[\x1F\x02\x0F\x16]/g, "");
    topic = topic.replace(/\x03\d{1,2}(?:,\d{1,2})?/g, "");

    this.name  = name;
    this.users = users;
    this.topic = topic;
}

ChannelEntry.prototype = new XULTreeViewRecord(channelTreeShare);

ChannelEntry.prototype.sortCompare =
function chanentry_sortcmp(a, b)
{
    var sc = a._share.sortColumn;
    var sd = a._share.sortDirection;

    // Make sure the special 'first' row is always first.
    if ("first" in a)
        return -1;
    if ("first" in b)
        return 1;

    if (sc == "users")
    {
        // Force a numeric comparison.
        a = 1 * a[sc];
        b = 1 * b[sc];
    }
    else
    {
        // Case-insensitive, please.
        a = a[sc].toLowerCase();
        b = b[sc].toLowerCase();
    }

    if (a < b)
        return -1 * sd;

    if (a > b)
        return 1 * sd;

    return 0;
}
