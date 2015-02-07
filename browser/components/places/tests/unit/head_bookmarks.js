/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/LoadContextInfo.jsm");

// Import common head.
let commonFile = do_get_file("../../../../../toolkit/components/places/tests/head_common.js", false);
if (commonFile) {
  let uri = Services.io.newFileURI(commonFile);
  Services.scriptloader.loadSubScript(uri.spec, this);
}

// Put any other stuff relative to this test folder below.

XPCOMUtils.defineLazyGetter(this, "PlacesUIUtils", function() {
  Cu.import("resource:///modules/PlacesUIUtils.jsm");
  return PlacesUIUtils;
});

const ORGANIZER_FOLDER_ANNO = "PlacesOrganizer/OrganizerFolder";
const ORGANIZER_QUERY_ANNO = "PlacesOrganizer/OrganizerQuery";

// Needed by some test that relies on having an app registered.
let XULAppInfo = {
  vendor: "Mozilla",
  name: "PlacesTest",
  ID: "{230de50e-4cd1-11dc-8314-0800200c9a66}",
  version: "1",
  appBuildID: "2007010101",
  platformVersion: "",
  platformBuildID: "2007010101",
  inSafeMode: false,
  logConsoleErrors: true,
  OS: "XPCShell",
  XPCOMABI: "noarch-spidermonkey",

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsIXULAppInfo,
    Ci.nsIXULRuntime,
  ])
};

let XULAppInfoFactory = {
  createInstance: function (outer, iid) {
    if (outer != null)
      throw Cr.NS_ERROR_NO_AGGREGATION;
    return XULAppInfo.QueryInterface(iid);
  }
};
let registrar = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
registrar.registerFactory(Components.ID("{fbfae60b-64a4-44ef-a911-08ceb70b9f31}"),
                          "XULAppInfo", "@mozilla.org/xre/app-info;1",
                          XULAppInfoFactory);

// Smart bookmarks constants.
const SMART_BOOKMARKS_VERSION = 7;
const SMART_BOOKMARKS_ON_TOOLBAR = 1;
const SMART_BOOKMARKS_ON_MENU =  3; // Takes into account the additional separator.

// Default bookmarks constants.
const DEFAULT_BOOKMARKS_ON_TOOLBAR = 1;
const DEFAULT_BOOKMARKS_ON_MENU = 1;

const SMART_BOOKMARKS_ANNO = "Places/SmartBookmark";

function checkItemHasAnnotation(guid, name) {
  return PlacesUtils.promiseItemId(guid).then(id => {
    let hasAnnotation = PlacesUtils.annotations.itemHasAnnotation(id, name);
    Assert.ok(hasAnnotation, `Expected annotation ${name}`);
  });
}

function waitForImportAndSmartBookmarks() {
  return Promise.all([
    promiseTopicObserved("bookmarks-restore-success"),
    PlacesTestUtils.promiseAsyncUpdates()
  ]);
}

function promiseEndUpdateBatch() {
  return new Promise(resolve => {
    PlacesUtils.bookmarks.addObserver({
      __proto__: NavBookmarkObserver.prototype,
      onEndUpdateBatch: resolve
    }, false);
  });
}

let createCorruptDB = Task.async(function* () {
  let dbPath = OS.Path.join(OS.Constants.Path.profileDir, "places.sqlite");
  yield OS.File.remove(dbPath);

  // Create a corrupt database.
  let dir = yield OS.File.getCurrentDirectory();
  let src = OS.Path.join(dir, "corruptDB.sqlite");
  yield OS.File.copy(src, dbPath);

  // Check there's a DB now.
  Assert.ok((yield OS.File.exists(dbPath)), "should have a DB now");
});
