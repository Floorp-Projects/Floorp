/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let Scope = {};
Cu.import("resource://gre/modules/Task.jsm", Scope);
Cu.import("resource://gre/modules/Promise.jsm", Scope);
let {Task, Promise} = Scope;


// This tests that a tab which is closed while loading is not lost.
// Specifically, that session store does not rely on an invalid cache when
// constructing data for a tab which is loading.

// This test steps through the following parts:
//  1. Tab has been created is loading URI_TO_LOAD.
//  2. Before URI_TO_LOAD finishes loading, browser.currentURI has changed and
//     tab is scheduled to be removed.
//  3. After the tab has been closed, undoCloseTab() has been called and the tab
//     should fully load.
const URI_TO_LOAD = "about:mozilla";

function waitForLoadStarted(aTab) {
  let deferred = Promise.defer();
  waitForContentMessage(aTab.linkedBrowser,
    "SessionStore:loadStart",
    1000,
    deferred.resolve);
  return deferred.promise;
}

function waitForTabLoaded(aTab) {
  let deferred = Promise.defer();
  whenBrowserLoaded(aTab.linkedBrowser, deferred.resolve);
  return deferred.promise;
}

function waitForTabClosed() {
  let deferred = Promise.defer();
  let observer = function() {
    gBrowser.tabContainer.removeEventListener("TabClose", observer, true);
    deferred.resolve();
  };
  gBrowser.tabContainer.addEventListener("TabClose", observer, true);
  return deferred.promise;
}

function test() {
  waitForExplicitFinish();

  Task.spawn(function() {
    try {
      // Open a new tab
      let tab = gBrowser.addTab("about:blank");
      yield waitForTabLoaded(tab);

      // Trigger a save state, to initialize any caches
      ss.getBrowserState();

      is(gBrowser.tabs[1], tab, "newly created tab should exist by now");
      ok(tab.linkedBrowser.__SS_data, "newly created tab should be in save state");

      // Start a load and interrupt it by closing the tab
      tab.linkedBrowser.loadURI(URI_TO_LOAD);
      let loaded = yield waitForLoadStarted(tab);
      ok(loaded, "Load started");

      let tabClosing = waitForTabClosed();
      gBrowser.removeTab(tab);
      info("Now waiting for TabClose to close");
      yield tabClosing;

      // Undo the tab, ensure that it proceeds with loading
      tab = ss.undoCloseTab(window, 0);
      yield waitForTabLoaded(tab);
      is(tab.linkedBrowser.currentURI.spec, URI_TO_LOAD, "loading proceeded as expected");

      gBrowser.removeTab(tab);

      executeSoon(finish);
    } catch (ex) {
      ok(false, ex);
      info(ex.stack);
    }
  });
}
