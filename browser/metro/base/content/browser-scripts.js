// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

/*
 * JS modules
 */

XPCOMUtils.defineLazyModuleGetter(this, "Downloads",
                                  "resource://gre/modules/Downloads.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "FormHistory",
                                  "resource://gre/modules/FormHistory.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "FileUtils",
                                  "resource://gre/modules/FileUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PageThumbs",
                                  "resource://gre/modules/PageThumbs.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PluralForm",
                                  "resource://gre/modules/PluralForm.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
                                  "resource://gre/modules/PlacesUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
                                  "resource://gre/modules/NetUtil.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "DownloadUtils",
                                  "resource://gre/modules/DownloadUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "NewTabUtils",
                                  "resource://gre/modules/NewTabUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Promise",
                                  "resource://gre/modules/commonjs/sdk/core/promise.js");

XPCOMUtils.defineLazyModuleGetter(this, "Task",
                                  "resource://gre/modules/Task.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "OS",
                                  "resource://gre/modules/osfile.jsm");

/*
 * Services
 */

XPCOMUtils.defineLazyServiceGetter(this, "StyleSheetSvc",
                                   "@mozilla.org/content/style-sheet-service;1",
                                   "nsIStyleSheetService");
XPCOMUtils.defineLazyServiceGetter(window, "gHistSvc",
                                   "@mozilla.org/browser/nav-history-service;1",
                                   "nsINavHistoryService",
                                   "nsIBrowserHistory");
XPCOMUtils.defineLazyServiceGetter(window, "gURIFixup",
                                   "@mozilla.org/docshell/urifixup;1",
                                   "nsIURIFixup");
XPCOMUtils.defineLazyServiceGetter(window, "gFaviconService",
                                   "@mozilla.org/browser/favicon-service;1",
                                   "nsIFaviconService");
XPCOMUtils.defineLazyServiceGetter(window, "gFocusManager",
                                   "@mozilla.org/focus-manager;1",
                                   "nsIFocusManager");
#ifdef MOZ_CRASHREPORTER
XPCOMUtils.defineLazyServiceGetter(this, "CrashReporter",
                                   "@mozilla.org/xre/app-info;1",
                                   "nsICrashReporter");
#endif

/*
 * window.Rect is used by
 * http://www.w3.org/TR/DOM-Level-2-Style/css.html#CSS-Rect
 * so it is not possible to set a lazy getter for Geometry.jsm.
 */
Cu.import("resource://gre/modules/Geometry.jsm");
/*
 * Browser scripts
 */
let ScriptContexts = {};
[
  ["ContentAreaObserver", "chrome://browser/content/ContentAreaObserver.js"],
  ["WebProgress", "chrome://browser/content/WebProgress.js"],
  ["FindHelperUI", "chrome://browser/content/helperui/FindHelperUI.js"],
  ["FormHelperUI", "chrome://browser/content/helperui/FormHelperUI.js"],
  ["BrowserTouchHandler", "chrome://browser/content/BrowserTouchHandler.js"],
  ["AlertsHelper", "chrome://browser/content/helperui/AlertsHelper.js"],
  ["AutofillMenuUI", "chrome://browser/content/helperui/MenuUI.js"],
  ["ContextMenuUI", "chrome://browser/content/helperui/MenuUI.js"],
  ["MenuControlUI", "chrome://browser/content/helperui/MenuUI.js"],
  ["MenuPopup", "chrome://browser/content/helperui/MenuUI.js"],
  ["IndexedDB", "chrome://browser/content/helperui/IndexedDB.js"],
  ["OfflineApps", "chrome://browser/content/helperui/OfflineApps.js"],
  ["SelectHelperUI", "chrome://browser/content/helperui/SelectHelperUI.js"],
  ["SelectionHelperUI", "chrome://browser/content/helperui/SelectionHelperUI.js"],
  ["SelectionPrototype", "chrome://browser/content/library/SelectionPrototype.js"],
  ["ChromeSelectionHandler", "chrome://browser/content/helperui/ChromeSelectionHandler.js"],
  ["AnimatedZoom", "chrome://browser/content/AnimatedZoom.js"],
  ["CommandUpdater", "chrome://browser/content/commandUtil.js"],
  ["ContextCommands", "chrome://browser/content/ContextCommands.js"],
  ["Bookmarks", "chrome://browser/content/bookmarks.js"],
  ["MetroDownloadsView", "chrome://browser/content/downloads.js"],
  ["ConsolePanelView", "chrome://browser/content/console.js"],
  ["Site", "chrome://browser/content/Site.js"],
  ["TopSites", "chrome://browser/content/TopSites.js"],
  ["Sanitizer", "chrome://browser/content/sanitize.js"],
  ["SanitizeUI", "chrome://browser/content/sanitizeUI.js"],
  ["SSLExceptions", "chrome://browser/content/exceptions.js"],
  ["ItemPinHelper", "chrome://browser/content/helperui/ItemPinHelper.js"],
  ["NavButtonSlider", "chrome://browser/content/NavButtonSlider.js"],
  ["ContextUI", "chrome://browser/content/ContextUI.js"],
  ["FlyoutPanelsUI", "chrome://browser/content/flyoutpanels/FlyoutPanelsUI.js"],
  ["APZCObserver", "chrome://browser/content/apzc.js"],
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
#ifdef MOZ_SERVICES_SYNC
XPCOMUtils.defineLazyGetter(this, "Weave", function() {
  Components.utils.import("resource://services-sync/main.js");
  return Weave;
});
#endif

/*
 * Delay load some global scripts using a custom namespace
 */
XPCOMUtils.defineLazyGetter(this, "GlobalOverlay", function() {
  let GlobalOverlay = {};
  Services.scriptloader.loadSubScript("chrome://global/content/globalOverlay.js", GlobalOverlay);
  return GlobalOverlay;
});

XPCOMUtils.defineLazyGetter(this, "ContentAreaUtils", function() {
  let ContentAreaUtils = {};
  Services.scriptloader.loadSubScript("chrome://global/content/contentAreaUtils.js", ContentAreaUtils);
  return ContentAreaUtils;
});

XPCOMUtils.defineLazyGetter(this, "ZoomManager", function() {
  let sandbox = {};
  Services.scriptloader.loadSubScript("chrome://global/content/viewZoomOverlay.js", sandbox);
  return sandbox.ZoomManager;
});
