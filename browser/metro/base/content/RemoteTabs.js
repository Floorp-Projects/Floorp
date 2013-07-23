// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';
Components.utils.import("resource://services-sync/main.js");

/**
 * Wraps a list/grid control implementing nsIDOMXULSelectControlElement and
 * fills it with the user's synced tabs.
 *
 * Note, the Sync module takes care of initializing the sync service. We should
 * not make calls that start sync or sync tabs since this module loads really
 * early during startup.
 *
 * @param    aSet         Control implementing nsIDOMXULSelectControlElement.
 * @param    aSetUIAccess The UI element that should be hidden when Sync is
 *                          disabled. Must sanely support 'hidden' attribute.
 *                          You may only have one UI access point at this time.
 */
function RemoteTabsView(aSet, aSetUIAccessList) {
  this._set = aSet;
  this._set.controller = this;
  this._uiAccessElements = aSetUIAccessList;

  // Sync uses special voodoo observers.
  // If you want to change this code, talk to the fx-si team
  Weave.Svc.Obs.add("weave:service:sync:finish", this);
  Weave.Svc.Obs.add("weave:service:start-over", this);
  if (this.isSyncEnabled() ) {
    this.populateGrid();
  }
  else {
    this.setUIAccessVisible(false);
  }
}

RemoteTabsView.prototype = {
  _set: null,
  _uiAccessElements: [],

  handleItemClick: function tabview_handleItemClick(aItem) {
    let url = aItem.getAttribute("value");
    BrowserUI.goToURI(url);
  },

  observe: function(subject, topic, data) {
    switch (topic) {
      case "weave:service:sync:finish":
        this.populateGrid();
        break;
      case "weave:service:start-over":
        this.setUIAccessVisible(false);
        break;
    }
  },

  setUIAccessVisible: function setUIAccessVisible(aVisible) {
    for (let elem of this._uiAccessElements) {
      elem.hidden = !aVisible;
    }
  },

  populateGrid: function populateGrid() {

    let tabsEngine = Weave.Service.engineManager.get("tabs");
    let list = this._set;
    let seenURLs = new Set();

    // Clear grid, We don't know what has happened to tabs since last sync
    // Also can result in duplicate tabs(bug 864614)
    this._set.clearAll();
    let show = false;
    for (let [guid, client] in Iterator(tabsEngine.getAllClients())) {
      client.tabs.forEach(function({title, urlHistory, icon}) {
        let url = urlHistory[0];
        if (tabsEngine.locallyOpenTabMatchesURL(url) || seenURLs.has(url)) {
          return;
        }
        seenURLs.add(url);
        show = true;

        // If we wish to group tabs by client, we should be looking for records
        //  of {type:client, clientName, class:{mobile, desktop}} and will
        //  need to readd logic to reset seenURLs for each client.

        let item = this._set.appendItem((title || url), url);
        item.setAttribute("iconURI", Weave.Utils.getIcon(icon));

      }, this);
    }
    this.setUIAccessVisible(show);
  },

  destruct: function destruct() {
    Weave.Svc.Obs.remove("weave:engine:sync:finish", this);
    Weave.Svc.Obs.remove("weave:service:logout:start-over", this);
  },

  isSyncEnabled: function isSyncEnabled() {
    return (Weave.Status.checkSetup() != Weave.CLIENT_NOT_CONFIGURED);
  }

};

let RemoteTabsStartView = {
  _view: null,
  get _grid() { return document.getElementById("start-remotetabs-grid"); },

  init: function init() {
    let vbox = document.getElementById("start-remotetabs");
    let uiList = [vbox];
    this._view = new RemoteTabsView(this._grid, uiList);
  },

  uninit: function uninit() {
    this._view.destruct();
  },

  show: function show() {
    this._grid.arrangeItems();
  }
};
