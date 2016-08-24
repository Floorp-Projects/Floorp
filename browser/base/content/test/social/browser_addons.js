var AddonManager = Cu.import("resource://gre/modules/AddonManager.jsm", {}).AddonManager;
var SocialService = Cu.import("resource:///modules/SocialService.jsm", {}).SocialService;

var manifest = {
  name: "provider 1",
  origin: "https://example.com",
  shareURL: "https://example.com/browser/browser/base/content/test/social/social_share.html",
  iconURL: "https://example.com/browser/browser/base/content/test/general/moz.png"
};
var manifest2 = { // used for testing install
  name: "provider 2",
  origin: "https://test1.example.com",
  shareURL: "https://test1.example.com/browser/browser/base/content/test/social/social_share.html",
  iconURL: "https://test1.example.com/browser/browser/base/content/test/general/moz.png",
  version: "1.0"
};
var manifestUpgrade = { // used for testing install
  name: "provider 3",
  origin: "https://test2.example.com",
  shareURL: "https://test2.example.com/browser/browser/base/content/test/social/social_share.html",
  iconURL: "https://test2.example.com/browser/browser/base/content/test/general/moz.png",
  version: "1.0"
};

function test() {
  waitForExplicitFinish();
  PopupNotifications.panel.setAttribute("animate", "false");
  registerCleanupFunction(function () {
    PopupNotifications.panel.removeAttribute("animate");
  });

  let prefname = getManifestPrefname(manifest);
  // ensure that manifest2 is NOT showing as builtin
  is(SocialService.getOriginActivationType(manifest.origin), "foreign", "manifest is foreign");
  is(SocialService.getOriginActivationType(manifest2.origin), "foreign", "manifest2 is foreign");

  Services.prefs.setBoolPref("social.remote-install.enabled", true);
  runSocialTests(tests, undefined, undefined, function () {
    Services.prefs.clearUserPref("social.remote-install.enabled");
    ok(!Services.prefs.prefHasUserValue(prefname), "manifest is not in user-prefs");
    // just in case the tests failed, clear these here as well
    Services.prefs.clearUserPref("social.directories");
    finish();
  });
}

function installListener(next, aManifest) {
  let expectEvent = "onInstalling";
  let prefname = getManifestPrefname(aManifest);
  // wait for the actual removal to call next
  SocialService.registerProviderListener(function providerListener(topic, origin, providers) {
    if (topic == "provider-disabled") {
      SocialService.unregisterProviderListener(providerListener);
      is(origin, aManifest.origin, "provider disabled");
      executeSoon(next);
    }
  });

  return {
    onInstalling: function(addon) {
      is(expectEvent, "onInstalling", "install started");
      is(addon.manifest.origin, aManifest.origin, "provider about to be installed");
      ok(!Services.prefs.prefHasUserValue(prefname), "manifest is not in user-prefs");
      expectEvent = "onInstalled";
    },
    onInstalled: function(addon) {
      is(addon.manifest.origin, aManifest.origin, "provider installed");
      ok(addon.installDate.getTime() > 0, "addon has installDate");
      ok(addon.updateDate.getTime() > 0, "addon has updateDate");
      ok(Services.prefs.prefHasUserValue(prefname), "manifest is in user-prefs");
      expectEvent = "onUninstalling";
    },
    onUninstalling: function(addon) {
      is(expectEvent, "onUninstalling", "uninstall started");
      is(addon.manifest.origin, aManifest.origin, "provider about to be uninstalled");
      ok(Services.prefs.prefHasUserValue(prefname), "manifest is in user-prefs");
      expectEvent = "onUninstalled";
    },
    onUninstalled: function(addon) {
      is(expectEvent, "onUninstalled", "provider has been uninstalled");
      is(addon.manifest.origin, aManifest.origin, "provider uninstalled");
      ok(!Services.prefs.prefHasUserValue(prefname), "manifest is not in user-prefs");
      AddonManager.removeAddonListener(this);
    }
  };
}

var tests = {
  testHTTPInstallFailure: function(next) {
    let installFrom = "http://example.com";
    is(SocialService.getOriginActivationType(installFrom), "foreign", "testing foriegn install");
    let data = {
      origin: installFrom,
      url: installFrom+"/activate",
      manifest: manifest,
      window: window
    }
    Social.installProvider(data, function(addonManifest) {
      ok(!addonManifest, "unable to install provider over http");
      next();
    });
  },
  testAddonEnableToggle: function(next) {
    let expectEvent;
    let prefname = getManifestPrefname(manifest);
    let listener = {
      onEnabled: function(addon) {
        is(expectEvent, "onEnabled", "provider onEnabled");
        ok(!addon.userDisabled, "provider enabled");
        executeSoon(function() {
          expectEvent = "onDisabling";
          addon.userDisabled = true;
        });
      },
      onEnabling: function(addon) {
        is(expectEvent, "onEnabling", "provider onEnabling");
        expectEvent = "onEnabled";
      },
      onDisabled: function(addon) {
        is(expectEvent, "onDisabled", "provider onDisabled");
        ok(addon.userDisabled, "provider disabled");
        AddonManager.removeAddonListener(listener);
        // clear the provider user-level pref
        Services.prefs.clearUserPref(prefname);
        executeSoon(next);
      },
      onDisabling: function(addon) {
        is(expectEvent, "onDisabling", "provider onDisabling");
        expectEvent = "onDisabled";
      }
    };
    AddonManager.addAddonListener(listener);

    // we're only testing enable disable, so we quickly set the user-level pref
    // for this provider and test enable/disable toggling
    setManifestPref(prefname, manifest);
    ok(Services.prefs.prefHasUserValue(prefname), "manifest is in user-prefs");
    AddonManager.getAddonsByTypes(["service"], function(addons) {
      for (let addon of addons) {
        if (addon.userDisabled) {
          expectEvent = "onEnabling";
          addon.userDisabled = false;
          // only test with one addon
          return;
        }
      }
      ok(false, "no addons toggled");
      next();
    });
  },
  testProviderEnableToggle: function(next) {
    // enable and disabel a provider from the SocialService interface, check
    // that the addon manager is updated

    let expectEvent;
    let prefname = getManifestPrefname(manifest);

    let listener = {
      onEnabled: function(addon) {
        is(expectEvent, "onEnabled", "provider onEnabled");
        is(addon.manifest.origin, manifest.origin, "provider enabled");
        ok(!addon.userDisabled, "provider !userDisabled");
      },
      onEnabling: function(addon) {
        is(expectEvent, "onEnabling", "provider onEnabling");
        is(addon.manifest.origin, manifest.origin, "provider about to be enabled");
        expectEvent = "onEnabled";
      },
      onDisabled: function(addon) {
        is(expectEvent, "onDisabled", "provider onDisabled");
        is(addon.manifest.origin, manifest.origin, "provider disabled");
        ok(addon.userDisabled, "provider userDisabled");
      },
      onDisabling: function(addon) {
        is(expectEvent, "onDisabling", "provider onDisabling");
        is(addon.manifest.origin, manifest.origin, "provider about to be disabled");
        expectEvent = "onDisabled";
      }
    };
    AddonManager.addAddonListener(listener);

    expectEvent = "onEnabling";
    setManifestPref(prefname, manifest);
    SocialService.enableProvider(manifest.origin, function(provider) {
      expectEvent = "onDisabling";
      SocialService.disableProvider(provider.origin, function() {
        AddonManager.removeAddonListener(listener);
        Services.prefs.clearUserPref(prefname);
        next();
      });
    });
  },
  testDirectoryInstall: function(next) {
    AddonManager.addAddonListener(installListener(next, manifest2));

    BrowserTestUtils.waitForEvent(PopupNotifications.panel, "popupshown").then(() => {
      let panel = document.getElementById("servicesInstall-notification");
      info("servicesInstall-notification panel opened");
      panel.button.click();
    });

    let activationURL = manifest2.origin + "/browser/browser/base/content/test/social/social_activate.html"
    Services.prefs.setCharPref("social.directories", manifest2.origin);
    is(SocialService.getOriginActivationType(manifest2.origin), "directory", "testing directory install");
    let data = {
      origin: manifest2.origin,
      url: manifest2.origin + "/directory",
      manifest: manifest2,
      window: window
    }
    Social.installProvider(data, function(addonManifest) {
      Services.prefs.clearUserPref("social.directories");
      SocialService.enableProvider(addonManifest.origin, function(provider) {
        Social.uninstallProvider(addonManifest.origin);
      });
    });
  }
}
