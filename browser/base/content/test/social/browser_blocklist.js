/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// a place for miscellaneous social tests

let SocialService = Cu.import("resource://gre/modules/SocialService.jsm", {}).SocialService;

const URI_EXTENSION_BLOCKLIST_DIALOG = "chrome://mozapps/content/extensions/blocklist.xul";
let blocklistURL = "http://test:80/browser/browser/base/content/test/social/blocklist.xml";
let blocklistEmpty = "http://test:80/browser/browser/base/content/test/social/blocklistEmpty.xml";

let manifest = { // normal provider
  name: "provider ok",
  origin: "https://example.com",
  sidebarURL: "https://example.com/browser/browser/base/content/test/social/social_sidebar.html",
  workerURL: "https://example.com/browser/browser/base/content/test/social/social_worker.js",
  iconURL: "https://example.com/browser/browser/base/content/test/moz.png"
};
let manifest_bad = { // normal provider
  name: "provider blocked",
  origin: "https://test1.example.com",
  sidebarURL: "https://test1.example.com/browser/browser/base/content/test/social/social_sidebar.html",
  workerURL: "https://test1.example.com/browser/browser/base/content/test/social/social_worker.js",
  iconURL: "https://test1.example.com/browser/browser/base/content/test/moz.png"
};

function test() {
  waitForExplicitFinish();

  runSocialTests(tests, undefined, undefined, function () {
    resetBlocklist(finish); //restore to original pref
  });
}

var tests = {
  testSimpleBlocklist: function(next) {
    // this really just tests adding and clearing our blocklist for later tests
    setAndUpdateBlocklist(blocklistURL, function() {
      ok(Services.blocklist.isAddonBlocklisted("test1.example.com@services.mozilla.org", "0", "0", "0"), "blocking 'blocked'");
      ok(!Services.blocklist.isAddonBlocklisted("example.com@services.mozilla.org", "0", "0", "0"), "not blocking 'good'");
      setAndUpdateBlocklist(blocklistEmpty, function() {
        ok(!Services.blocklist.isAddonBlocklisted("test1.example.com@services.mozilla.org", "0", "0", "0"), "blocklist cleared");
        next();
      });
    });
  },
  testAddingNonBlockedProvider: function(next) {
    function finish(isgood) {
      ok(isgood, "adding non-blocked provider ok");
      Services.prefs.clearUserPref("social.manifest.good");
      setAndUpdateBlocklist(blocklistEmpty, next);
    }
    setManifestPref("social.manifest.good", manifest);
    setAndUpdateBlocklist(blocklistURL, function() {
      try {
        SocialService.addProvider(manifest, function(provider) {
          if (provider) {
            SocialService.removeProvider(provider.origin, function() {
              ok(true, "added and removed provider");
              finish(true);
            });
          } else {
            finish(false);
          }
        });
      } catch(e) {
        dump(e+" - "+e.stack+"\n");
        finish(false);
      }
    });
  },
  testAddingBlockedProvider: function(next) {
    function finish(good) {
      ok(good, "Unable to add blocklisted provider");
      Services.prefs.clearUserPref("social.manifest.blocked");
      setAndUpdateBlocklist(blocklistEmpty, next);
    }
    setManifestPref("social.manifest.blocked", manifest_bad);
    setAndUpdateBlocklist(blocklistURL, function() {
      try {
        SocialService.addProvider(manifest_bad, function(provider) {
          if (provider) {
            SocialService.removeProvider(provider.origin, function() {
              finish(false);
            });
          } else {
            finish(true);
          }
        });
      } catch(e) {
        finish(true);
      }
    });
  },
  testInstallingBlockedProvider: function(next) {
    function finish(good) {
      ok(good, "Unable to add blocklisted provider");
      Services.prefs.clearUserPref("social.whitelist");
      setAndUpdateBlocklist(blocklistEmpty, next);
    }
    let activationURL = manifest_bad.origin + "/browser/browser/base/content/test/social/social_activate.html"
    addTab(activationURL, function(tab) {
      let doc = tab.linkedBrowser.contentDocument;
      let installFrom = doc.nodePrincipal.origin;
      // whitelist to avoid the 3rd party install dialog, we only want to test
      // the blocklist inside installProvider.
      Services.prefs.setCharPref("social.whitelist", installFrom);
      setAndUpdateBlocklist(blocklistURL, function() {
        try {
          // expecting an exception when attempting to install a hard blocked
          // provider
          Social.installProvider(doc, manifest_bad, function(addonManifest) {
            gBrowser.removeTab(tab);
            finish(false);
          });
        } catch(e) {
          gBrowser.removeTab(tab);
          finish(true);
        }
      });
    });
  },
  testBlockingExistingProvider: function(next) {

    addWindowListener(URI_EXTENSION_BLOCKLIST_DIALOG,  function(win) {
      win.close();
      ok(true, "window closed");
    });

    setManifestPref("social.manifest.blocked", manifest_bad);
    SocialService.addProvider(manifest_bad, function(provider) {
      if (provider) {
        // the act of blocking should cause a 'provider-removed' notification
        // from SocialService.
        SocialService.registerProviderListener(function providerListener() {
          SocialService.unregisterProviderListener(providerListener);
          SocialService.getProvider(provider.origin, function(p) {
            ok(p==null, "blocklisted provider removed");
            Services.prefs.clearUserPref("social.manifest.blocked");
            setAndUpdateBlocklist(blocklistEmpty, next);
          });
        });
        // no callback - the act of updating should cause the listener above
        // to fire.
        setAndUpdateBlocklist(blocklistURL);
      }
    });
  }
}
