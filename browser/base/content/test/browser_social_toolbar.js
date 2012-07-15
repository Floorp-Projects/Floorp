/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let SocialService = Cu.import("resource://gre/modules/SocialService.jsm", {}).SocialService;
let gProvider;

function test() {
  waitForExplicitFinish();

  Services.prefs.setBoolPref("social.enabled", true);
  registerCleanupFunction(function () {
    Services.prefs.clearUserPref("social.enabled");
  });

  let oldProvider;
  function saveOldProviderAndStartTestWith(provider) {
    oldProvider = Social.provider;
    registerCleanupFunction(function () {
      Social.provider = oldProvider;
    });
    Social.provider = gProvider = provider;
    runTests(tests, undefined, undefined, function () {
      SocialService.removeProvider(provider.origin, finish);
    });
  }

  let manifest = { // normal provider
    name: "provider 1",
    origin: "https://example1.com",
    workerURL: "https://example1.com/worker.js",
    iconURL: "chrome://branding/content/icon48.png"
  };
  SocialService.addProvider(manifest, function(provider) {
    // If the UI is already active, run the test immediately, otherwise wait
    // for initialization.
    if (Social.provider) {
      saveOldProviderAndStartTestWith(provider);
    } else {
      Services.obs.addObserver(function obs() {
        Services.obs.removeObserver(obs, "test-social-ui-ready");
        saveOldProviderAndStartTestWith(provider);
      }, "test-social-ui-ready", false);
    }
  });
}

var tests = {
  testProfileSet: function(next) {
    let profile = {
      portrait: "chrome://branding/content/icon48.png",
      userName: "trickster",
      displayName: "Kuma Lisa",
      profileURL: "http://en.wikipedia.org/wiki/Kuma_Lisa"
    }
    gProvider.updateUserProfile(profile);
    // check dom values
    let portrait = document.getElementById("social-statusarea-user-portrait").getAttribute("src");
    is(portrait, profile.portrait, "portrait is set");
    let userButton = document.getElementById("social-statusarea-username");
    ok(!userButton.hidden, "username is visible");
    is(userButton.label, profile.userName, "username is set");
    next();
  },
  testAmbientNotifications: function(next) {
    let ambience = {
      name: "testIcon",
      iconURL: "chrome://branding/content/icon48.png",
      contentPanel: "about:blank",
      counter: 42
    };
    gProvider.setAmbientNotification(ambience);

    let statusIcons = document.getElementById("social-status-iconbox");
    ok(!statusIcons.firstChild.collapsed, "status icon is visible");
    ok(!statusIcons.firstChild.lastChild.collapsed, "status value is visible");
    is(statusIcons.firstChild.lastChild.textContent, "42", "status value is correct");

    ambience.counter = 0;
    gProvider.setAmbientNotification(ambience);
    ok(statusIcons.firstChild.lastChild.collapsed, "status value is not visible");
    is(statusIcons.firstChild.lastChild.textContent, "", "status value is correct");
    next();
  },
  testProfileUnset: function(next) {
    gProvider.updateUserProfile({});
    // check dom values
    let portrait = document.getElementById("social-statusarea-user-portrait").getAttribute("src");
    is(portrait, "chrome://browser/skin/social/social.png", "portrait is generic");
    let userButton = document.getElementById("social-statusarea-username");
    ok(userButton.hidden, "username is not visible");
    let ambience = document.getElementById("social-status-iconbox").firstChild;
    while (ambience) {
      ok(ambience.collapsed, "ambient icon is collapsed");
      ambience = ambience.nextSibling;
    }
    
    next();
  }
}

function runTests(tests, cbPreTest, cbPostTest, cbFinish) {
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
