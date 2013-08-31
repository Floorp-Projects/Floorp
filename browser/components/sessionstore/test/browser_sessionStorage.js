/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let Scope = {};
Cu.import("resource://gre/modules/Task.jsm", Scope);
Cu.import("resource://gre/modules/Promise.jsm", Scope);
let {Task, Promise} = Scope;

function promiseBrowserLoaded(aBrowser) {
  let deferred = Promise.defer();
  whenBrowserLoaded(aBrowser, () => deferred.resolve());
  return deferred.promise;
}

function forceWriteState() {
  let deferred = Promise.defer();
  const PREF = "browser.sessionstore.interval";
  const TOPIC = "sessionstore-state-write";

  Services.obs.addObserver(function observe() {
    Services.obs.removeObserver(observe, TOPIC);
    Services.prefs.clearUserPref(PREF);
    deferred.resolve();
  }, TOPIC, false);

  Services.prefs.setIntPref(PREF, 0);
  return deferred.promise;
}

function waitForStorageChange(aTab) {
  let deferred = Promise.defer();
  waitForContentMessage(aTab.linkedBrowser,
    "SessionStore:MozStorageChanged",
    1000,
    deferred.resolve);
  return deferred.promise;
}

function test() {

  waitForExplicitFinish();

  let tab;
  Task.spawn(function() {
    try {
      let SESSION_STORAGE_KEY = "SESSION_STORAGE_KEY " + Math.random();
      let SESSION_STORAGE_VALUE = "SESSION_STORAGE_VALUE " + Math.random();
      let LOCAL_STORAGE_KEY = "LOCAL_STORAGE_KEY " + Math.random();
      let LOCAL_STORAGE_VALUE = "LOCAL_STORAGE_VALUE " + Math.random();

      tab = gBrowser.addTab("http://example.com");
      // about:home supports sessionStorage and localStorage

      let win = tab.linkedBrowser.contentWindow;

      // Flush loading and next save, call getBrowserState()
      // a few times to ensure that everything is cached.
      yield promiseBrowserLoaded(tab.linkedBrowser);
      yield forceWriteState();
      info("Calling getBrowserState() to populate cache");
      ss.getBrowserState();

      info("Change sessionStorage, ensure that state is saved");
      let storageChangedPromise = waitForStorageChange(tab);
      win.sessionStorage[SESSION_STORAGE_KEY] = SESSION_STORAGE_VALUE;
      let storageChanged = yield storageChangedPromise;
      ok(storageChanged, "Changing sessionStorage triggered the right message");
      yield forceWriteState();

      let state = ss.getBrowserState();
      ok(state.indexOf(SESSION_STORAGE_KEY) != -1, "Key appears in state");
      ok(state.indexOf(SESSION_STORAGE_VALUE) != -1, "Value appears in state");


      info("Change localStorage, ensure that state is not saved");
      storageChangedPromise = waitForStorageChange(tab);
      win.localStorage[LOCAL_STORAGE_KEY] = LOCAL_STORAGE_VALUE;
      storageChanged = yield storageChangedPromise;
      ok(!storageChanged, "Changing localStorage did not trigger a message");
      yield forceWriteState();

      state = ss.getBrowserState();
      ok(state.indexOf(LOCAL_STORAGE_KEY) == -1, "Key does not appear in state");
      ok(state.indexOf(LOCAL_STORAGE_VALUE) == -1, "Value does not appear in state");
    } catch (ex) {
      ok(false, ex);
      info(ex.stack);
    } finally {
      // clean up
      if (tab) {
        gBrowser.removeTab(tab);
      }

      executeSoon(finish);
    }
  });
}
