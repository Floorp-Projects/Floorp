

let AddonManager = Cu.import("resource://gre/modules/AddonManager.jsm", {}).AddonManager;
let SocialService = Cu.import("resource://gre/modules/SocialService.jsm", {}).SocialService;

const ADDON_TYPE_SERVICE     = "service";
const ID_SUFFIX              = "@services.mozilla.org";
const STRING_TYPE_NAME       = "type.%ID%.name";
const XPINSTALL_URL = "chrome://mozapps/content/xpinstall/xpinstallConfirm.xul";

let manifest = { // builtin provider
  name: "provider 1",
  origin: "https://example.com",
  sidebarURL: "https://example.com/browser/browser/base/content/test/social/social_sidebar.html",
  workerURL: "https://example.com/browser/browser/base/content/test/social/social_worker.js",
  iconURL: "https://example.com/browser/browser/base/content/test/moz.png"
};
let manifest2 = { // used for testing install
  name: "provider 2",
  origin: "https://test1.example.com",
  sidebarURL: "https://test1.example.com/browser/browser/base/content/test/social/social_sidebar.html",
  workerURL: "https://test1.example.com/browser/browser/base/content/test/social/social_worker.js",
  iconURL: "https://test1.example.com/browser/browser/base/content/test/moz.png",
  version: 1
};

function test() {
  waitForExplicitFinish();

  let prefname = getManifestPrefname(manifest);
  setBuiltinManifestPref(prefname, manifest);
  // ensure that manifest2 is NOT showing as builtin
  is(SocialService.getOriginActivationType(manifest.origin), "builtin", "manifest is builtin");
  is(SocialService.getOriginActivationType(manifest2.origin), "foreign", "manifest2 is not builtin");

  Services.prefs.setBoolPref("social.remote-install.enabled", true);
  runSocialTests(tests, undefined, undefined, function () {
    Services.prefs.clearUserPref("social.remote-install.enabled");
    // clear our builtin pref
    ok(!Services.prefs.prefHasUserValue(prefname), "manifest is not in user-prefs");
    resetBuiltinManifestPref(prefname);
    // just in case the tests failed, clear these here as well
    Services.prefs.clearUserPref("social.whitelist");
    Services.prefs.clearUserPref("social.directories");
    finish();
  });
}

function installListener(next, aManifest) {
  let expectEvent = "onInstalling";
  let prefname = getManifestPrefname(aManifest);
  // wait for the actual removal to call next
  SocialService.registerProviderListener(function providerListener(topic, data) {
    if (topic == "provider-removed") {
      SocialService.unregisterProviderListener(providerListener);
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
    AddonManager.getAddonsByTypes([ADDON_TYPE_SERVICE], function(addons) {
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
    SocialService.addBuiltinProvider(manifest.origin, function(provider) {
      expectEvent = "onDisabling";
      SocialService.removeProvider(provider.origin, function() {
        AddonManager.removeAddonListener(listener);
        Services.prefs.clearUserPref(prefname);
        next();
      });
    });
  },
  testForeignInstall: function(next) {
    AddonManager.addAddonListener(installListener(next, manifest2));

    // we expect the addon install dialog to appear, we need to accept the
    // install from the dialog.
    info("Waiting for install dialog");
    let panel = document.getElementById("servicesInstall-notification");
    PopupNotifications.panel.addEventListener("popupshown", function onpopupshown() {
      PopupNotifications.panel.removeEventListener("popupshown", onpopupshown);
      info("servicesInstall-notification panel opened");
      panel.button.click();
    })

    let activationURL = manifest2.origin + "/browser/browser/base/content/test/social/social_activate.html"
    addTab(activationURL, function(tab) {
      let doc = tab.linkedBrowser.contentDocument;
      let installFrom = doc.nodePrincipal.origin;
      Services.prefs.setCharPref("social.whitelist", "");
      is(SocialService.getOriginActivationType(installFrom), "foreign", "testing foriegn install");
      Social.installProvider(doc, manifest2, function(addonManifest) {
        Services.prefs.clearUserPref("social.whitelist");
        SocialService.addBuiltinProvider(addonManifest.origin, function(provider) {
          Social.uninstallProvider(addonManifest.origin);
          gBrowser.removeTab(tab);
        });
      });
    });
  },
  testBuiltinInstallWithoutManifest: function(next) {
    // send installProvider null for the manifest
    AddonManager.addAddonListener(installListener(next, manifest));

    let prefname = getManifestPrefname(manifest);
    let activationURL = manifest.origin + "/browser/browser/base/content/test/social/social_activate.html"
    addTab(activationURL, function(tab) {
      let doc = tab.linkedBrowser.contentDocument;
      let installFrom = doc.nodePrincipal.origin;
      is(SocialService.getOriginActivationType(installFrom), "builtin", "testing builtin install");
      ok(!Services.prefs.prefHasUserValue(prefname), "manifest is not in user-prefs");
      Social.installProvider(doc, null, function(addonManifest) {
        ok(Services.prefs.prefHasUserValue(prefname), "manifest is in user-prefs");
        SocialService.addBuiltinProvider(addonManifest.origin, function(provider) {
          Social.uninstallProvider(addonManifest.origin);
          gBrowser.removeTab(tab);
        });
      });
    });
  },
  testBuiltinInstall: function(next) {
    // send installProvider a json object for the manifest
    AddonManager.addAddonListener(installListener(next, manifest));

    let prefname = getManifestPrefname(manifest);
    let activationURL = manifest.origin + "/browser/browser/base/content/test/social/social_activate.html"
    addTab(activationURL, function(tab) {
      let doc = tab.linkedBrowser.contentDocument;
      let installFrom = doc.nodePrincipal.origin;
      is(SocialService.getOriginActivationType(installFrom), "builtin", "testing builtin install");
      ok(!Services.prefs.prefHasUserValue(prefname), "manifest is not in user-prefs");
      Social.installProvider(doc, manifest, function(addonManifest) {
        ok(Services.prefs.prefHasUserValue(prefname), "manifest is in user-prefs");
        SocialService.addBuiltinProvider(addonManifest.origin, function(provider) {
          Social.uninstallProvider(addonManifest.origin);
          gBrowser.removeTab(tab);
        });
      });
    });
  },
  testWhitelistInstall: function(next) {
    AddonManager.addAddonListener(installListener(next, manifest2));

    let activationURL = manifest2.origin + "/browser/browser/base/content/test/social/social_activate.html"
    addTab(activationURL, function(tab) {
      let doc = tab.linkedBrowser.contentDocument;
      let installFrom = doc.nodePrincipal.origin;
      Services.prefs.setCharPref("social.whitelist", installFrom);
      is(SocialService.getOriginActivationType(installFrom), "whitelist", "testing whitelist install");
      Social.installProvider(doc, manifest2, function(addonManifest) {
        Services.prefs.clearUserPref("social.whitelist");
        SocialService.addBuiltinProvider(addonManifest.origin, function(provider) {
          Social.uninstallProvider(addonManifest.origin);
          gBrowser.removeTab(tab);
        });
      });
    });
  },
  testDirectoryInstall: function(next) {
    AddonManager.addAddonListener(installListener(next, manifest2));

    let activationURL = manifest2.origin + "/browser/browser/base/content/test/social/social_activate.html"
    addTab(activationURL, function(tab) {
      let doc = tab.linkedBrowser.contentDocument;
      let installFrom = doc.nodePrincipal.origin;
      Services.prefs.setCharPref("social.directories", installFrom);
      is(SocialService.getOriginActivationType(installFrom), "directory", "testing directory install");
      Social.installProvider(doc, manifest2, function(addonManifest) {
        Services.prefs.clearUserPref("social.directories");
        SocialService.addBuiltinProvider(addonManifest.origin, function(provider) {
          Social.uninstallProvider(addonManifest.origin);
          gBrowser.removeTab(tab);
        });
      });
    });
  },
  testUpgradeProviderFromWorker: function(next) {
    // add the provider, change the pref, add it again. The provider at that
    // point should be upgraded
    let activationURL = manifest2.origin + "/browser/browser/base/content/test/social/social_activate.html"
    addTab(activationURL, function(tab) {
      let doc = tab.linkedBrowser.contentDocument;
      let installFrom = doc.nodePrincipal.origin;
      Services.prefs.setCharPref("social.whitelist", installFrom);
      Social.installProvider(doc, manifest2, function(addonManifest) {
        SocialService.addBuiltinProvider(addonManifest.origin, function(provider) {
          is(provider.manifest.version, 1, "manifest version is 1");
          Social.enabled = true;

          // watch for the provider-update and test the new version
          SocialService.registerProviderListener(function providerListener(topic, data) {
            if (topic != "provider-update")
              return;
            SocialService.unregisterProviderListener(providerListener);
            Services.prefs.clearUserPref("social.whitelist");
            let provider = Social._getProviderFromOrigin(addonManifest.origin);
            is(provider.manifest.version, 2, "manifest version is 2");
            Social.uninstallProvider(addonManifest.origin);
            gBrowser.removeTab(tab);
            next();
          });

          let port = provider.getWorkerPort();
          port.onmessage = function (e) {
            let topic = e.data.topic;
            switch (topic) {
              case "got-sidebar-message":
                ok(true, "got the sidebar message from provider 1");
                port.postMessage({topic: "worker.update", data: true});
                break;
            }
          };
          port.postMessage({topic: "test-init"});

        });
      });
    });
  }
}
