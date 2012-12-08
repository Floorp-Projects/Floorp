/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function waitForCondition(condition, nextTest, errorMsg) {
  var tries = 0;
  var interval = setInterval(function() {
    if (tries >= 30) {
      ok(false, errorMsg);
      moveOn();
    }
    if (condition()) {
      moveOn();
    }
    tries++;
  }, 100);
  var moveOn = function() { clearInterval(interval); nextTest(); };
}

// Check that a specified (string) URL hasn't been "remembered" (ie, is not
// in history, will not appear in about:newtab or auto-complete, etc.)
function ensureSocialUrlNotRemembered(url) {
  let gh = Cc["@mozilla.org/browser/global-history;2"]
           .getService(Ci.nsIGlobalHistory2);
  let uri = Services.io.newURI(url, null, null);
  ok(!gh.isVisited(uri), "social URL " + url + " should not be in global history");
}

function runSocialTestWithProvider(manifest, callback) {
  let SocialService = Cu.import("resource://gre/modules/SocialService.jsm", {}).SocialService;

  let manifests = Array.isArray(manifest) ? manifest : [manifest];

  // Check that none of the provider's content ends up in history.
  registerCleanupFunction(function () {
    manifests.forEach(function (m) {
      for (let what of ['sidebarURL', 'workerURL', 'iconURL']) {
        if (m[what]) {
          ensureSocialUrlNotRemembered(m[what]);
        }
      }
    });
  });

  info("runSocialTestWithProvider: " + manifests.toSource());

  let providersAdded = 0;
  let firstProvider;
  manifests.forEach(function (m) {
    SocialService.addProvider(m, function(provider) {
      provider.active = true;

      providersAdded++;
      info("runSocialTestWithProvider: provider added");

      // we want to set the first specified provider as the UI's provider
      if (provider.origin == manifests[0].origin) {
        firstProvider = provider;
      }

      // If we've added all the providers we need, call the callback to start
      // the tests (and give it a callback it can call to finish them)
      if (providersAdded == manifests.length) {
        // Set the UI's provider and enable the feature
        Social.provider = firstProvider;
        Social.enabled = true;

        registerCleanupFunction(function () {
          // disable social before removing the providers to avoid providers
          // being activated immediately before we get around to removing it.
          Services.prefs.clearUserPref("social.enabled");
          // if one test happens to fail, it is likely finishSocialTest will not
          // be called, causing most future social tests to also fail as they
          // attempt to add a provider which already exists - so work
          // around that by also attempting to remove the test provider.
          manifests.forEach(function (m) {
            try {
              SocialService.removeProvider(m.origin, finish);
            } catch (ex) {}
          });
        });
        function finishSocialTest() {
          SocialService.removeProvider(provider.origin, finish);
        }
        callback(finishSocialTest);
      }
    });
  });
}

function runSocialTests(tests, cbPreTest, cbPostTest, cbFinish) {
  let testIter = Iterator(tests);

  if (cbPreTest === undefined) {
    cbPreTest = function(cb) {cb()};
  }
  if (cbPostTest === undefined) {
    cbPostTest = function(cb) {cb()};
  }

  function runNextTest() {
    let name, func;
    try {
      [name, func] = testIter.next();
    } catch (err if err instanceof StopIteration) {
      // out of items:
      (cbFinish || finish)();
      return;
    }
    // We run on a timeout as the frameworker also makes use of timeouts, so
    // this helps keep the debug messages sane.
    executeSoon(function() {
      function cleanupAndRunNextTest() {
        info("sub-test " + name + " complete");
        cbPostTest(runNextTest);
      }
      cbPreTest(function() {
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
