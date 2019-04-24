/* -*- indent-tabs-mode: nil; js-indent-level: 2; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cc, Ci } = require("chrome");
const {LocalizationHelper} = require("devtools/shared/l10n");
const MENUS_L10N = new LocalizationHelper("devtools/client/locales/menus.properties");

function l10n(key) {
  return MENUS_L10N.getStr(key);
}

const ChromeUtils = require("ChromeUtils");
ChromeUtils.defineModuleGetter(this, "Services",
                               "resource://gre/modules/Services.jsm");

const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyModuleGetters(this, {
  E10SUtils: "resource://gre/modules/E10SUtils.jsm",
});

function RecordNewTab() {
  const { gBrowser } = Services.wm.getMostRecentWindow("navigator:browser");
  gBrowser.selectedTab = gBrowser.addWebTab("about:blank", { recordExecution: "*" });
  Services.telemetry.scalarAdd("devtools.webreplay.new_recording", 1);
}

function ReloadAndRecordTab() {
  const { gBrowser } = Services.wm.getMostRecentWindow("navigator:browser");
  const url = gBrowser.currentURI.spec;
  gBrowser.updateBrowserRemoteness(gBrowser.selectedBrowser, {
    recordExecution: "*", newFrameloader: true,
    remoteType: E10SUtils.DEFAULT_REMOTE_TYPE,
  });
  gBrowser.loadURI(url, {
    triggeringPrincipal: gBrowser.selectedBrowser.contentPrincipal,
  });
  Services.telemetry.scalarAdd("devtools.webreplay.reload_recording", 1);
}

function ReloadAndStopRecordingTab() {
  const { gBrowser } = Services.wm.getMostRecentWindow("navigator:browser");
  const url = gBrowser.currentURI.spec;
  gBrowser.updateBrowserRemoteness(gBrowser.selectedBrowser, {
    newFrameloader: true,
    remoteType: E10SUtils.DEFAULT_REMOTE_TYPE,
  });
  gBrowser.loadURI(url, {
    triggeringPrincipal: gBrowser.selectedBrowser.contentPrincipal,
  });
  Services.telemetry.scalarAdd("devtools.webreplay.stop_recording", 1);
}

function SaveRecording() {
  const { gBrowser } = Services.wm.getMostRecentWindow("navigator:browser");
  const fp = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
  const window = gBrowser.ownerGlobal;
  fp.init(window, null, Ci.nsIFilePicker.modeSave);
  fp.open(rv => {
    if (rv == Ci.nsIFilePicker.returnOK || rv == Ci.nsIFilePicker.returnReplace) {
      const remoteTab = gBrowser.selectedTab.linkedBrowser.frameLoader.remoteTab;
      if (!remoteTab || !remoteTab.saveRecording(fp.file.path)) {
        window.alert("Current tab is not recording");
      }
    }
  });
  Services.telemetry.scalarAdd("devtools.webreplay.save_recording", 1);
}

function ReplayNewTab() {
  const { gBrowser } = Services.wm.getMostRecentWindow("navigator:browser");
  const fp = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
  const window = gBrowser.ownerGlobal;
  fp.init(window, null, Ci.nsIFilePicker.modeOpen);
  fp.open(rv => {
    if (rv == Ci.nsIFilePicker.returnOK || rv == Ci.nsIFilePicker.returnReplace) {
      gBrowser.selectedTab = gBrowser.addWebTab(null, { replayExecution: fp.file.path });
    }
  });
  Services.telemetry.scalarAdd("devtools.webreplay.load_recording", 1);
}

const menuItems = [
  { id: "devtoolsRecordNewTab", command: RecordNewTab },
  { id: "devtoolsReloadAndRecordTab", command: ReloadAndRecordTab },
  { id: "devtoolsSaveRecording", command: SaveRecording },
  { id: "devtoolsReplayNewTab", command: ReplayNewTab },
];

exports.addWebReplayMenu = function(doc) {
  const menu = doc.createXULElement("menu");
  menu.id = "menu_webreplay";
  menu.setAttribute("label", l10n("devtoolsWebReplay.label"));
  menu.setAttribute("hidden", "true");

  const popup = doc.createXULElement("menupopup");
  popup.id = "menupopup_webreplay";
  menu.appendChild(popup);

  for (const { id, command } of menuItems) {
    const menuitem = doc.createXULElement("menuitem");
    menuitem.id = id;
    menuitem.setAttribute("label", l10n(id + ".label"));
    menuitem.addEventListener("command", command);
    popup.appendChild(menuitem);
  }

  const mds = doc.getElementById("menu_devtools_separator");
  if (mds) {
    mds.parentNode.insertBefore(menu, mds);
  }
};

exports.reloadAndRecordTab = ReloadAndRecordTab;
exports.reloadAndStopRecordingTab = ReloadAndStopRecordingTab;
