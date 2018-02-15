/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let gSiteDataRemoveSelected = {

  init() {
    let bundlePreferences = document.getElementById("bundlePreferences");
    let acceptBtn = document.getElementById("SiteDataRemoveSelectedDialog")
                            .getButton("accept");
    acceptBtn.label = bundlePreferences.getString("acceptRemove");

    let hosts = window.arguments[0].hosts;
    hosts.sort();
    let tree = document.getElementById("sitesTree");
    this._view._hosts = hosts;
    tree.view = this._view;
  },

  ondialogaccept() {
    window.arguments[0].allowed = true;
  },

  ondialogcancel() {
    window.arguments[0].allowed = false;
  },

  _view: {
    _hosts: null,

    get rowCount() {
      return this._hosts.length;
    },
    getCellText(index, column) {
      return this._hosts[index];
    },
    getLevel(index) {
      return 0;
    },
  },
};
