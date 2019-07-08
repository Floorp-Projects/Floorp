/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
Services.scriptloader.loadSubScript(
  "chrome://global/content/globalOverlay.js",
  this
);
Services.scriptloader.loadSubScript(
  "chrome://browser/content/utilityOverlay.js",
  this
);

ChromeUtils.defineModuleGetter(
  this,
  "PlacesTestUtils",
  "resource://testing-common/PlacesTestUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "BrowserTestUtils",
  "resource://testing-common/BrowserTestUtils.jsm"
);

ChromeUtils.defineModuleGetter(
  window,
  "PlacesUtils",
  "resource://gre/modules/PlacesUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  window,
  "PlacesUIUtils",
  "resource:///modules/PlacesUIUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  window,
  "PlacesTransactions",
  "resource://gre/modules/PlacesTransactions.jsm"
);

var { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
XPCOMUtils.defineLazyScriptGetter(
  window,
  ["PlacesTreeView"],
  "chrome://browser/content/places/treeView.js"
);
XPCOMUtils.defineLazyScriptGetter(
  window,
  ["PlacesInsertionPoint", "PlacesController", "PlacesControllerDragHelper"],
  "chrome://browser/content/places/controller.js"
);
