/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let Cc = Components.classes;
let Ci = Components.interfaces;
let Cu = Components.utils;
let Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
                                  "resource://gre/modules/PlacesUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
                                  "resource://gre/modules/NetUtil.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "NewTabUtils",
                                  "resource://gre/modules/NewTabUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Promise",
                                  "resource://gre/modules/commonjs/sdk/core/promise.js");

XPCOMUtils.defineLazyModuleGetter(this, "Task",
                                  "resource://gre/modules/Task.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "CrossSlide",
                                  "resource:///modules/CrossSlide.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "OS",
                                  "resource://gre/modules/osfile.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "View",
                                  "resource:///modules/View.jsm");

XPCOMUtils.defineLazyServiceGetter(window, "gHistSvc",
                                   "@mozilla.org/browser/nav-history-service;1",
                                   "nsINavHistoryService",
                                   "nsIBrowserHistory");

let ScriptContexts = {};
[
  ["Util", "chrome://browser/content/Util.js"],
  ["Site", "chrome://browser/content/Site.js"],
  ["StartUI", "chrome://browser/content/StartUI.js"],
  ["Bookmarks", "chrome://browser/content/bookmarks.js"],
  ["BookmarksView", "chrome://browser/content/BookmarksView.js"],
  ["HistoryView", "chrome://browser/content/HistoryView.js"],
  ["TopSitesView", "chrome://browser/content/TopSitesView.js"],
  ["RemoteTabsView", "chrome://browser/content/RemoteTabsView.js"],
  ["BookmarksStartView", "chrome://browser/content/BookmarksView.js"],
  ["HistoryStartView", "chrome://browser/content/HistoryView.js"],
  ["TopSitesStartView", "chrome://browser/content/TopSitesView.js"],
  ["RemoteTabsStartView", "chrome://browser/content/RemoteTabsView.js"],
  ["ItemPinHelper", "chrome://browser/content/helperui/ItemPinHelper.js"],
].forEach(function (aScript) {
  let [name, script] = aScript;
  XPCOMUtils.defineLazyGetter(window, name, function() {
    let sandbox;
    if (script in ScriptContexts) {
      sandbox = ScriptContexts[script];
    } else {
      sandbox = ScriptContexts[script] = {};
      Services.scriptloader.loadSubScript(script, sandbox);
    }
    return sandbox[name];
  });
});

// singleton
XPCOMUtils.defineLazyGetter(this, "TopSites", function() {
  return StartUI.chromeWin.TopSites;
});

#ifdef MOZ_SERVICES_SYNC
XPCOMUtils.defineLazyGetter(this, "Weave", function() {
  Components.utils.import("resource://services-sync/main.js");
  return Weave;
});
#endif
