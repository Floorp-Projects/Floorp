/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// a place for miscellaneous social tests

var SocialService = Cu.import("resource://gre/modules/SocialService.jsm", {}).SocialService;

const URI_EXTENSION_BLOCKLIST_DIALOG = "chrome://mozapps/content/extensions/blocklist.xul";
var blocklistURL = "http://example.com/browser/browser/base/content/test/social/blocklist.xml";

var manifest = { // normal provider
  name: "provider ok",
  origin: "https://example.com",
  sidebarURL: "https://example.com/browser/browser/base/content/test/social/social_sidebar.html",
  iconURL: "https://example.com/browser/browser/base/content/test/general/moz.png"
};
var manifest_bad = { // normal provider
  name: "provider blocked",
  origin: "https://test1.example.com",
  sidebarURL: "https://test1.example.com/browser/browser/base/content/test/social/social_sidebar.html",
  iconURL: "https://test1.example.com/browser/browser/base/content/test/general/moz.png"
};

// blocklist testing
function updateBlocklist() {
  var blocklistNotifier = Cc["@mozilla.org/extensions/blocklist;1"]
                          .getService(Ci.nsITimerCallback);
  let promise = promiseObserverNotified("blocklist-updated");
  blocklistNotifier.notify(null);
  return promise;
}

var _originalTestBlocklistURL = null;
function setAndUpdateBlocklist(aURL) {
  if (!_originalTestBlocklistURL)
    _originalTestBlocklistURL = Services.prefs.getCharPref("extensions.blocklist.url");
  Services.prefs.setCharPref("extensions.blocklist.url", aURL);
  return updateBlocklist();
}

function resetBlocklist() {
  // XXX - this has "forked" from the head.js helpers in our parent directory :(
  // But let's reuse their blockNoPlugins.xml.  Later, we should arrange to
  // use their head.js helpers directly
  let noBlockedURL = "http://example.com/browser/browser/base/content/test/plugins/blockNoPlugins.xml";
  return new Promise(resolve => {
    setAndUpdateBlocklist(noBlockedURL).then(() => {
      Services.prefs.setCharPref("extensions.blocklist.url", _originalTestBlocklistURL);
      resolve();
    });
  });
}

function test() {
  waitForExplicitFinish();
  // turn on logging for nsBlocklistService.js
  Services.prefs.setBoolPref("extensions.logging.enabled", true);
  registerCleanupFunction(function () {
    Services.prefs.clearUserPref("extensions.logging.enabled");
  });

  runSocialTests(tests, undefined, undefined, function () {
    resetBlocklist().then(finish); //restore to original pref
  });
}

var tests = {
  testSimpleBlocklist: function(next) {
    // this really just tests adding and clearing our blocklist for later tests
    setAndUpdateBlocklist(blocklistURL).then(() => {
      ok(Services.blocklist.isAddonBlocklisted(SocialService.createWrapper(manifest_bad)), "blocking 'blocked'");
      ok(!Services.blocklist.isAddonBlocklisted(SocialService.createWrapper(manifest)), "not blocking 'good'");
      resetBlocklist().then(() => {
        ok(!Services.blocklist.isAddonBlocklisted(SocialService.createWrapper(manifest_bad)), "blocklist cleared");
        next();
      });
    });
  },
  testAddingNonBlockedProvider: function(next) {
    function finishTest(isgood) {
      ok(isgood, "adding non-blocked provider ok");
      Services.prefs.clearUserPref("social.manifest.good");
      resetBlocklist().then(next);
    }
    setManifestPref("social.manifest.good", manifest);
    setAndUpdateBlocklist(blocklistURL).then(() => {
      try {
        SocialService.addProvider(manifest, function(provider) {
          try {
            SocialService.disableProvider(provider.origin, function() {
              ok(true, "added and removed provider");
              finishTest(true);
            });
          } catch(e) {
            ok(false, "SocialService.disableProvider threw exception: " + e);
            finishTest(false);
          }
        });
      } catch(e) {
        ok(false, "SocialService.addProvider threw exception: " + e);
        finishTest(false);
      }
    });
  },
  testAddingBlockedProvider: function(next) {
    function finishTest(good) {
      ok(good, "Unable to add blocklisted provider");
      Services.prefs.clearUserPref("social.manifest.blocked");
      resetBlocklist().then(next);
    }
    setManifestPref("social.manifest.blocked", manifest_bad);
    setAndUpdateBlocklist(blocklistURL).then(() => {
      try {
        SocialService.addProvider(manifest_bad, function(provider) {
          SocialService.disableProvider(provider.origin, function() {
            ok(false, "SocialService.addProvider should throw blocklist exception");
            finishTest(false);
          });
        });
      } catch(e) {
        ok(true, "SocialService.addProvider should throw blocklist exception: " + e);
        finishTest(true);
      }
    });
  },
  testInstallingBlockedProvider: function(next) {
    function finishTest(good) {
      ok(good, "Unable to install blocklisted provider");
      resetBlocklist().then(next);
    }
    let activationURL = manifest_bad.origin + "/browser/browser/base/content/test/social/social_activate.html"
    setAndUpdateBlocklist(blocklistURL).then(() => {
      try {
        // expecting an exception when attempting to install a hard blocked
        // provider
        let data = {
          origin: manifest_bad.origin,
          url: activationURL,
          manifest: manifest_bad,
          window: window
        }
        Social.installProvider(data, function(addonManifest) {
          finishTest(false);
        });
      } catch(e) {
        finishTest(true);
      }
    });
  },
  testBlockingExistingProvider: function(next) {
    let listener = {
      _window: null,
      onOpenWindow: function(aXULWindow) {
        Services.wm.removeListener(this);
        this._window = aXULWindow;
        let domwindow = aXULWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                                  .getInterface(Ci.nsIDOMWindow);

        domwindow.addEventListener("load", function _load() {
          domwindow.removeEventListener("load", _load, false);

          domwindow.addEventListener("unload", function _unload() {
            domwindow.removeEventListener("unload", _unload, false);
            info("blocklist window was closed");
            Services.wm.removeListener(listener);
            next();
          }, false);

          is(domwindow.document.location.href, URI_EXTENSION_BLOCKLIST_DIALOG, "dialog opened and focused");
          // wait until after load to cancel so the dialog has initalized. we
          // don't want to accept here since that restarts the browser.
          executeSoon(() => {
            let cancelButton = domwindow.document.documentElement.getButton("cancel");
            info("***** hit the cancel button\n");
            cancelButton.doCommand();
          });
        }, false);
      },
      onCloseWindow: function(aXULWindow) { },
      onWindowTitleChange: function(aXULWindow, aNewTitle) { }
    };

    Services.wm.addListener(listener);

    setManifestPref("social.manifest.blocked", manifest_bad);
    try {
      SocialService.addProvider(manifest_bad, function(provider) {
        // the act of blocking should cause a 'provider-disabled' notification
        // from SocialService.
        SocialService.registerProviderListener(function providerListener(topic, origin, providers) {
          if (topic != "provider-disabled")
            return;
          SocialService.unregisterProviderListener(providerListener);
          is(origin, provider.origin, "provider disabled");
          SocialService.getProvider(provider.origin, function(p) {
            ok(p == null, "blocklisted provider disabled");
            Services.prefs.clearUserPref("social.manifest.blocked");
            resetBlocklist();
          });
        });
        // no callback - the act of updating should cause the listener above
        // to fire.
        setAndUpdateBlocklist(blocklistURL);
      });
    } catch(e) {
      ok(false, "unable to add provider " + e);
      next();
    }
  }
}
