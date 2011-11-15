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
 * The Original Code is Weave.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Edward Lee <edilee@mozilla.com>
 *   Mike Connor <mconnor@mozilla.com>
 *   Paul O’Shannessy <paul@oshannessy.com>
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


const Cu = Components.utils;

Cu.import("resource://services-sync/main.js");
Cu.import("resource:///modules/PlacesUIUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

let RemoteTabViewer = {
  _tabsList: null,

  init: function () {
    Services.obs.addObserver(this, "weave:service:login:finish", false);
    Services.obs.addObserver(this, "weave:engine:sync:finish", false);

    this._tabsList = document.getElementById("tabsList");

    this.buildList(true);
  },

  uninit: function () {
    Services.obs.removeObserver(this, "weave:service:login:finish");
    Services.obs.removeObserver(this, "weave:engine:sync:finish");
  },

  buildList: function(force) {
    if (!Weave.Service.isLoggedIn || !this._refetchTabs(force))
      return;
    //XXXzpao We should say something about not being logged in & not having data
    //        or tell the appropriate condition. (bug 583344)

    this._generateTabList();
  },

  createItem: function(attrs) {
    let item = document.createElement("richlistitem");

    // Copy the attributes from the argument into the item
    for (let attr in attrs)
      item.setAttribute(attr, attrs[attr]);

    if (attrs["type"] == "tab")
      item.label = attrs.title != "" ? attrs.title : attrs.url;

    return item;
  },

  filterTabs: function(event) {
    let val = event.target.value.toLowerCase();
    let numTabs = this._tabsList.getRowCount();
    let clientTabs = 0;
    let currentClient = null;
    for (let i = 0;i < numTabs;i++) {
      let item = this._tabsList.getItemAtIndex(i);
      let hide = false;
      if (item.getAttribute("type") == "tab") {
        if (item.getAttribute("url").toLowerCase().indexOf(val) == -1 &&
            item.getAttribute("title").toLowerCase().indexOf(val) == -1)
          hide = true;
        else
          clientTabs++;
      }
      else if (item.getAttribute("type") == "client") {
        if (currentClient) {
          if (clientTabs == 0)
            currentClient.hidden = true;
        }
        currentClient = item;
        clientTabs = 0;
      }
      item.hidden = hide;
    }
    if (clientTabs == 0)
      currentClient.hidden = true;
  },

  openSelected: function() {
    let items = this._tabsList.selectedItems;
    let urls = [];
    for (let i = 0;i < items.length;i++) {
      if (items[i].getAttribute("type") == "tab") {
        urls.push(items[i].getAttribute("url"));
        let index = this._tabsList.getIndexOfItem(items[i]);
        this._tabsList.removeItemAt(index);
      }
    }
    if (urls.length) {
      getTopWin().gBrowser.loadTabs(urls);
      this._tabsList.clearSelection();
    }
  },

  bookmarkSingleTab: function() {
    let item = this._tabsList.selectedItems[0];
    let uri = Weave.Utils.makeURI(item.getAttribute("url"));
    let title = item.getAttribute("title");
    PlacesUIUtils.showBookmarkDialog({ action: "add"
                                     , type: "bookmark"
                                     , uri: uri
                                     , title: title
                                     , hiddenRows: [ "description"
                                                   , "location"
                                                   , "folderPicker"
                                                   , "loadInSidebar"
                                                   , "keyword" ]
                                     }, window.top);
  },

  bookmarkSelectedTabs: function() {
    let items = this._tabsList.selectedItems;
    let URIs = [];
    for (let i = 0;i < items.length;i++) {
      if (items[i].getAttribute("type") == "tab") {
        let uri = Weave.Utils.makeURI(items[i].getAttribute("url"));
        if (!uri)
          continue;

        URIs.push(uri);
      }
    }
    if (URIs.length) {
      PlacesUIUtils.showBookmarkDialog({ action: "add"
                                       , type: "folder"
                                       , URIList: URIs
                                       , hiddenRows: [ "description" ]
                                       }, window.top);
    }
  },

  _generateTabList: function() {
    let engine = Weave.Engines.get("tabs");
    let list = this._tabsList;

    // clear out existing richlistitems
    let count = list.getRowCount();
    if (count > 0) {
      for (let i = count - 1; i >= 0; i--)
        list.removeItemAt(i);
    }

    for (let [guid, client] in Iterator(engine.getAllClients())) {
      // Create the client node, but don't add it in-case we don't show any tabs
      let appendClient = true;
      let seenURLs = {};
      client.tabs.forEach(function({title, urlHistory, icon}) {
        let url = urlHistory[0];
        if (engine.locallyOpenTabMatchesURL(url) || url in seenURLs)
          return;

        seenURLs[url] = null;

        if (appendClient) {
          let attrs = {
            type: "client",
            clientName: client.clientName,
            class: Weave.Clients.isMobile(client.id) ? "mobile" : "desktop"
          };
          let clientEnt = this.createItem(attrs);
          list.appendChild(clientEnt);
          appendClient = false;
          clientEnt.disabled = true;
        }
        let attrs = {
          type:  "tab",
          title: title || url,
          url:   url,
          icon:  Weave.Utils.getIcon(icon)
        }
        let tab = this.createItem(attrs);
        list.appendChild(tab);
      }, this);
    }
  },

  adjustContextMenu: function(event) {
    let mode = "all";
    switch (this._tabsList.selectedItems.length) {
      case 0:
        break;
      case 1:
        mode = "single"
        break;
      default:
        mode = "multiple";
        break;
    }
    let menu = document.getElementById("tabListContext");
    let el = menu.firstChild;
    while (el) {
      let showFor = el.getAttribute("showFor");
      if (showFor)
        el.hidden = showFor != mode && showFor != "all";

      el = el.nextSibling;
    }
  },

  _refetchTabs: function(force) {
    if (!force) {
      // Don't bother refetching tabs if we already did so recently
      let lastFetch = 0;
      try {
        lastFetch = Services.prefs.getIntPref("services.sync.lastTabFetch");
      }
      catch (e) { /* Just use the default value of 0 */ }
      let now = Math.floor(Date.now() / 1000);
      if (now - lastFetch < 30)
        return false;
    }

    // if Clients hasn't synced yet this session, need to sync it as well
    if (Weave.Clients.lastSync == 0)
      Weave.Clients.sync();

    // Force a sync only for the tabs engine
    let engine = Weave.Engines.get("tabs");
    engine.lastModified = null;
    engine.sync();
    Services.prefs.setIntPref("services.sync.lastTabFetch",
                              Math.floor(Date.now() / 1000));

    return true;
  },

  observe: function(subject, topic, data) {
    switch (topic) {
      case "weave:service:login:finish":
        this.buildList(true);
        break;
      case "weave:engine:sync:finish":
        if (subject == "tabs")
          this._generateTabList();
        break;
    }
  },

  handleClick: function(event) {
    if (event.target.getAttribute("type") != "tab")
      return;

    if (event.button == 1) {
      let url = event.target.getAttribute("url");
      openUILink(url, event);
      let index = this._tabsList.getIndexOfItem(event.target);
      this._tabsList.removeItemAt(index);
    }
  }
}

