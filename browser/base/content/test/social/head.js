/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Task",
  "resource://gre/modules/Task.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
  "resource://gre/modules/PlacesUtils.jsm");


function promiseObserverNotified(aTopic) {
  return new Promise(resolve => {
    Services.obs.addObserver(function onNotification(aSubject, aTopic, aData) {
      dump("notification promised "+aTopic);
      Services.obs.removeObserver(onNotification, aTopic);
      TestUtils.executeSoon(() => resolve({subject: aSubject, data: aData}));
    }, aTopic, false);
  });
}

// Check that a specified (string) URL hasn't been "remembered" (ie, is not
// in history, will not appear in about:newtab or auto-complete, etc.)
function promiseSocialUrlNotRemembered(url) {
  return new Promise(resolve => {
    let uri = Services.io.newURI(url, null, null);
    PlacesUtils.asyncHistory.isURIVisited(uri, function(aURI, aIsVisited) {
      ok(!aIsVisited, "social URL " + url + " should not be in global history");
      resolve();
    });
  });
}

var gURLsNotRemembered = [];


function checkProviderPrefsEmpty(isError) {
  let MANIFEST_PREFS = Services.prefs.getBranch("social.manifest.");
  let prefs = MANIFEST_PREFS.getChildList("", []);
  let c = 0;
  for (let pref of prefs) {
    if (MANIFEST_PREFS.prefHasUserValue(pref)) {
      info("provider [" + pref + "] manifest left installed from previous test");
      c++;
    }
  }
  is(c, 0, "all provider prefs uninstalled from previous test");
  is(Social.providers.length, 0, "all providers uninstalled from previous test " + Social.providers.length);
}

function defaultFinishChecks() {
  checkProviderPrefsEmpty(true);
  finish();
}

function runSocialTestWithProvider(manifest, callback, finishcallback) {

  let SocialService = Cu.import("resource://gre/modules/SocialService.jsm", {}).SocialService;

  let manifests = Array.isArray(manifest) ? manifest : [manifest];

  // Check that none of the provider's content ends up in history.
  function finishCleanUp() {
    for (let i = 0; i < manifests.length; i++) {
      let m = manifests[i];
      for (let what of ['iconURL', 'shareURL']) {
        if (m[what]) {
          yield promiseSocialUrlNotRemembered(m[what]);
        }
      };
    }
    for (let i = 0; i < gURLsNotRemembered.length; i++) {
      yield promiseSocialUrlNotRemembered(gURLsNotRemembered[i]);
    }
    gURLsNotRemembered = [];
  }

  info("runSocialTestWithProvider: " + manifests.toSource());

  let finishCount = 0;
  function finishIfDone(callFinish) {
    finishCount++;
    if (finishCount == manifests.length)
      Task.spawn(finishCleanUp).then(finishcallback || defaultFinishChecks);
  }
  function removeAddedProviders(cleanup) {
    manifests.forEach(function (m) {
      // If we're "cleaning up", don't call finish when done.
      let callback = cleanup ? function () {} : finishIfDone;
      // Similarly, if we're cleaning up, catch exceptions from removeProvider
      let removeProvider = SocialService.disableProvider.bind(SocialService);
      if (cleanup) {
        removeProvider = function (origin, cb) {
          try {
            SocialService.disableProvider(origin, cb);
          } catch (ex) {
            // Ignore "provider doesn't exist" errors.
            if (ex.message.indexOf("SocialService.disableProvider: no provider with origin") == 0)
              return;
            info("Failed to clean up provider " + origin + ": " + ex);
          }
        }
      }
      removeProvider(m.origin, callback);
    });
  }
  function finishSocialTest(cleanup) {
    removeAddedProviders(cleanup);
  }

  let providersAdded = 0;
  let firstProvider;

  manifests.forEach(function (m) {
    SocialService.addProvider(m, function(provider) {

      providersAdded++;
      info("runSocialTestWithProvider: provider added");

      // we want to set the first specified provider as the UI's provider
      if (provider.origin == manifests[0].origin) {
        firstProvider = provider;
      }

      // If we've added all the providers we need, call the callback to start
      // the tests (and give it a callback it can call to finish them)
      if (providersAdded == manifests.length) {
        registerCleanupFunction(function () {
          finishSocialTest(true);
        });
        BrowserTestUtils.waitForCondition(() => provider.enabled,
                                          "providers added and enabled").then(() => {
          info("provider has been enabled");
          callback(finishSocialTest);
        });
      }
    });
  });
}

function runSocialTests(tests, cbPreTest, cbPostTest, cbFinish) {
  let testIter = (function*() {
    for (let name in tests) {
      if (tests.hasOwnProperty(name)) {
        yield [name, tests[name]];
      }
    }
  })();
  let providersAtStart = Social.providers.length;
  info("runSocialTests: start test run with " + providersAtStart + " providers");
  window.focus();


  if (cbPreTest === undefined) {
    cbPreTest = function(cb) {cb()};
  }
  if (cbPostTest === undefined) {
    cbPostTest = function(cb) {cb()};
  }

  function runNextTest() {
    let result = testIter.next();
    if (result.done) {
      // out of items:
      (cbFinish || defaultFinishChecks)();
      is(providersAtStart, Social.providers.length,
         "runSocialTests: finish test run with " + Social.providers.length + " providers");
      return;
    }
    let [name, func] = result.value;
    // We run on a timeout to help keep the debug messages sane.
    executeSoon(function() {
      function cleanupAndRunNextTest() {
        info("sub-test " + name + " complete");
        cbPostTest(runNextTest);
      }
      cbPreTest(function() {
        info("pre-test: starting with " + Social.providers.length + " providers");
        info("sub-test " + name + " starting");
        try {
          func.call(tests, cleanupAndRunNextTest);
        } catch (ex) {
          ok(false, "sub-test " + name + " failed: " + ex.toString() +"\n"+ex.stack);
          cleanupAndRunNextTest();
        }
      })
    });
  }
  runNextTest();
}

// A fairly large hammer which checks all aspects of the SocialUI for
// internal consistency.
function checkSocialUI(win) {
  let SocialService = Cu.import("resource://gre/modules/SocialService.jsm", {}).SocialService;
  // if we have enabled providers, we should also have instances of those
  // providers
  if (SocialService.hasEnabledProviders) {
    ok(Social.providers.length > 0, "providers are enabled");
  } else {
    is(Social.providers.length, 0, "providers are not enabled");
  }
}

function setManifestPref(name, manifest) {
  let string = Cc["@mozilla.org/supports-string;1"].
               createInstance(Ci.nsISupportsString);
  string.data = JSON.stringify(manifest);
  Services.prefs.setComplexValue(name, Ci.nsISupportsString, string);
}

function getManifestPrefname(aManifest) {
  // is same as the generated name in SocialServiceInternal.getManifestPrefname
  let originUri = Services.io.newURI(aManifest.origin, null, null);
  return "social.manifest." + originUri.hostPort.replace('.','-');
}

function ensureFrameLoaded(frame, uri) {
  return new Promise(resolve => {
    if (frame.contentDocument && frame.contentDocument.readyState == "complete" &&
        (!uri || frame.contentDocument.location.href == uri)) {
      resolve();
    } else {
      frame.addEventListener("load", function handler() {
        if (uri && frame.contentDocument.location.href != uri)
          return;
        frame.removeEventListener("load", handler, true);
        resolve()
      }, true);
    }
  });
}

// Support for going on and offline.
// (via browser/base/content/test/browser_bookmark_titles.js)
var origProxyType = Services.prefs.getIntPref('network.proxy.type');

function toggleOfflineStatus(goOffline) {
  // Bug 968887 fix.  when going on/offline, wait for notification before continuing
  return new Promise(resolve => {
    if (!goOffline) {
      Services.prefs.setIntPref('network.proxy.type', origProxyType);
    }
    if (goOffline != Services.io.offline) {
      info("initial offline state " + Services.io.offline);
      let expect = !Services.io.offline;
      Services.obs.addObserver(function offlineChange(subject, topic, data) {
        Services.obs.removeObserver(offlineChange, "network:offline-status-changed");
        info("offline state changed to " + Services.io.offline);
        is(expect, Services.io.offline, "network:offline-status-changed successful toggle");
        resolve();
      }, "network:offline-status-changed", false);
      BrowserOffline.toggleOfflineStatus();
    } else {
      resolve();
    }
    if (goOffline) {
      Services.prefs.setIntPref('network.proxy.type', 0);
      // LOAD_FLAGS_BYPASS_CACHE isn't good enough. So clear the cache.
      Services.cache2.clear();
    }
  });
}

function goOffline() {
  // Simulate a network outage with offline mode. (Localhost is still
  // accessible in offline mode, so disable the test proxy as well.)
  return toggleOfflineStatus(true);
}

function goOnline(callback) {
  return toggleOfflineStatus(false);
}
