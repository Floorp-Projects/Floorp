/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let SocialService = Cu.import("resource://gre/modules/SocialService.jsm", {}).SocialService;

let tabsToRemove = [];

function postTestCleanup(callback) {
  Social.provider = null;
  // any tabs opened by the test.
  for (let tab of tabsToRemove)
    gBrowser.removeTab(tab);
  tabsToRemove = [];
  // theses tests use the notification panel but don't bother waiting for it
  // to fully open - the end result is that the panel might stay open
  SocialUI.activationPanel.hidePopup();

  Services.prefs.clearUserPref("social.whitelist");

  // all providers may have had their manifests added.
  for (let manifest of gProviders)
    Services.prefs.clearUserPref("social.manifest." + manifest.origin);
  // all the providers may have been added.
  let numRemoved = 0;
  let cbRemoved = function() {
    if (++numRemoved == gProviders.length) {
      executeSoon(function() {
        is(Social.providers.length, 0, "all providers removed");
        callback();
      });
    }
  }
  for (let [i, manifest] of Iterator(gProviders)) {
    try {
      SocialService.removeProvider(manifest.origin, cbRemoved);
    } catch(ex) {
      // this test didn't add this provider - that's ok.
      cbRemoved();
    }
  }
}

function addBuiltinManifest(manifest) {
  let prefname = getManifestPrefname(manifest);
  setBuiltinManifestPref(prefname, manifest);
  return prefname;
}

function addTab(url, callback) {
  let tab = gBrowser.selectedTab = gBrowser.addTab(url, {skipAnimation: true});
  tab.linkedBrowser.addEventListener("load", function tabLoad(event) {
    tab.linkedBrowser.removeEventListener("load", tabLoad, true);
    tabsToRemove.push(tab);
    executeSoon(function() {callback(tab)});
  }, true);
}

function sendActivationEvent(tab, callback, nullManifest) {
  // hack Social.lastEventReceived so we don't hit the "too many events" check.
  Social.lastEventReceived = 0;
  let doc = tab.linkedBrowser.contentDocument;
  // if our test has a frame, use it
  if (doc.defaultView.frames[0])
    doc = doc.defaultView.frames[0].document;
  let button = doc.getElementById(nullManifest ? "activation-old" : "activation");
  EventUtils.synthesizeMouseAtCenter(button, {}, doc.defaultView);
  executeSoon(callback);
}

function activateProvider(domain, callback, nullManifest) {
  let activationURL = domain+"/browser/browser/base/content/test/social/social_activate.html"
  addTab(activationURL, function(tab) {
    sendActivationEvent(tab, callback, nullManifest);
  });
}

function activateIFrameProvider(domain, callback) {
  let activationURL = domain+"/browser/browser/base/content/test/social/social_activate_iframe.html"
  addTab(activationURL, function(tab) {
    sendActivationEvent(tab, callback, false);
  });
}

function waitForProviderLoad(cb) {
  Services.obs.addObserver(function providerSet(subject, topic, data) {
    Services.obs.removeObserver(providerSet, "social:provider-set");
    info("social:provider-set observer was notified");
    waitForCondition(function() {
      let sbrowser = document.getElementById("social-sidebar-browser");
      return Social.provider.profile &&
             Social.provider.profile.displayName &&
             sbrowser.docShellIsActive;
    }, function() {
      // executeSoon to let the browser UI observers run first
      executeSoon(cb);
    },
    "waitForProviderLoad: provider profile was not set");
  }, "social:provider-set", false);
}


function getAddonItemInList(aId, aList) {
  var item = aList.firstChild;
  while (item) {
    if ("mAddon" in item && item.mAddon.id == aId) {
      aList.ensureElementIsVisible(item);
      return item;
    }
    item = item.nextSibling;
  }
  return null;
}

function clickAddonRemoveButton(tab, aCallback) {
  AddonManager.getAddonsByTypes(["service"], function(aAddons) {
    let addon = aAddons[0];

    let doc = tab.linkedBrowser.contentDocument;
    let list = doc.getElementById("addon-list");

    let item = getAddonItemInList(addon.id, list);
    isnot(item, null, "Should have found the add-on in the list");

    var button = doc.getAnonymousElementByAttribute(item, "anonid", "remove-btn");
    isnot(button, null, "Should have a remove button");
    ok(!button.disabled, "Button should not be disabled");

    EventUtils.synthesizeMouseAtCenter(button, { }, doc.defaultView);

    // Force XBL to apply
    item.clientTop;

    is(item.getAttribute("pending"), "uninstall", "Add-on should be uninstalling");

    executeSoon(function() { aCallback(addon); });
  });
}

function activateOneProvider(manifest, finishActivation, aCallback) {
  activateProvider(manifest.origin, function() {
    waitForProviderLoad(function() {
      ok(!SocialUI.activationPanel.hidden, "activation panel is showing");
      is(Social.provider.origin, manifest.origin, "new provider is active");
      checkSocialUI();

      if (finishActivation)
        document.getElementById("social-activation-button").click();
      else
        document.getElementById("social-undoactivation-button").click();

      executeSoon(aCallback);
    });
  });
}

let gTestDomains = ["https://example.com", "https://test1.example.com", "https://test2.example.com"];
let gProviders = [
  {
    name: "provider 1",
    origin: "https://example.com",
    sidebarURL: "https://example.com/browser/browser/base/content/test/social/social_sidebar.html?provider1",
    workerURL: "https://example.com/browser/browser/base/content/test/social/social_worker.js#no-profile,no-recommend",
    iconURL: "chrome://branding/content/icon48.png"
  },
  {
    name: "provider 2",
    origin: "https://test1.example.com",
    sidebarURL: "https://test1.example.com/browser/browser/base/content/test/social/social_sidebar.html?provider2",
    workerURL: "https://test1.example.com/browser/browser/base/content/test/social/social_worker.js#no-profile,no-recommend",
    iconURL: "chrome://branding/content/icon64.png"
  },
  {
    name: "provider 3",
    origin: "https://test2.example.com",
    sidebarURL: "https://test2.example.com/browser/browser/base/content/test/social/social_sidebar.html?provider2",
    workerURL: "https://test2.example.com/browser/browser/base/content/test/social/social_worker.js#no-profile,no-recommend",
    iconURL: "chrome://branding/content/about-logo.png"
  }
];


function test() {
  waitForExplicitFinish();
  runSocialTests(tests, undefined, postTestCleanup);
}

var tests = {
  testActivationWrongOrigin: function(next) {
    // At this stage none of our providers exist, so we expect failure.
    Services.prefs.setBoolPref("social.remote-install.enabled", false);
    activateProvider(gTestDomains[0], function() {
      is(SocialUI.enabled, false, "SocialUI is not enabled");
      ok(SocialUI.activationPanel.hidden, "activation panel still hidden");
      checkSocialUI();
      Services.prefs.clearUserPref("social.remote-install.enabled");
      next();
    });
  },

  testIFrameActivation: function(next) {
    Services.prefs.setCharPref("social.whitelist", gTestDomains.join(","));
    activateIFrameProvider(gTestDomains[0], function() {
      is(SocialUI.enabled, false, "SocialUI is not enabled");
      ok(!Social.provider, "provider is not installed");
      ok(SocialUI.activationPanel.hidden, "activation panel still hidden");
      checkSocialUI();
      Services.prefs.clearUserPref("social.whitelist");
      next();
    });
  },

  testActivationFirstProvider: function(next) {
    Services.prefs.setCharPref("social.whitelist", gTestDomains.join(","));
    // first up we add a manifest entry for a single provider.
    activateOneProvider(gProviders[0], false, function() {
      // we deactivated leaving no providers left, so Social is disabled.
      ok(!Social.provider, "should be no provider left after disabling");
      checkSocialUI();
      Services.prefs.clearUserPref("social.whitelist");
      next();
    });
  },

  testActivationBuiltin: function(next) {
    let prefname = addBuiltinManifest(gProviders[0]);
    is(SocialService.getOriginActivationType(gTestDomains[0]), "builtin", "manifest is builtin");
    // first up we add a manifest entry for a single provider.
    activateOneProvider(gProviders[0], false, function() {
      // we deactivated leaving no providers left, so Social is disabled.
      ok(!Social.provider, "should be no provider left after disabling");
      checkSocialUI();
      resetBuiltinManifestPref(prefname);
      next();
    });
  },

  testActivationMultipleProvider: function(next) {
    // The trick with this test is to make sure that Social.providers[1] is
    // the current provider when doing the undo - this makes sure that the
    // Social code doesn't fallback to Social.providers[0], which it will
    // do in some cases (but those cases do not include what this test does)
    // first enable the 2 providers
    Services.prefs.setCharPref("social.whitelist", gTestDomains.join(","));
    SocialService.addProvider(gProviders[0], function() {
      SocialService.addProvider(gProviders[1], function() {
        Social.provider = Social.providers[1];
        checkSocialUI();
        // activate the last provider.
        let prefname = addBuiltinManifest(gProviders[2]);
        activateOneProvider(gProviders[2], false, function() {
          // we deactivated - the first provider should be enabled.
          is(Social.provider.origin, Social.providers[1].origin, "original provider should have been reactivated");
          checkSocialUI();
          Services.prefs.clearUserPref("social.whitelist");
          resetBuiltinManifestPref(prefname);
          next();
        });
      });
    });
  },

  testRemoveNonCurrentProvider: function(next) {
    Services.prefs.setCharPref("social.whitelist", gTestDomains.join(","));
    SocialService.addProvider(gProviders[0], function() {
      SocialService.addProvider(gProviders[1], function() {
        Social.provider = Social.providers[1];
        checkSocialUI();
        // activate the last provider.
        addBuiltinManifest(gProviders[2]);
        activateProvider(gTestDomains[2], function() {
          waitForProviderLoad(function() {
            ok(!SocialUI.activationPanel.hidden, "activation panel is showing");
            is(Social.provider.origin, gTestDomains[2], "new provider is active");
            checkSocialUI();
            // A bit contrived, but set a new provider current while the
            // activation ui is up.
            Social.provider = Social.providers[1];
            // hit "undo"
            document.getElementById("social-undoactivation-button").click();
            executeSoon(function() {
              // we deactivated - the same provider should be enabled.
              is(Social.provider.origin, Social.providers[1].origin, "original provider still be active");
              checkSocialUI();
              Services.prefs.clearUserPref("social.whitelist");
              next();
            });
          });
        });
      });
    });
  },

  testAddonManagerDoubleInstall: function(next) {
    Services.prefs.setCharPref("social.whitelist", gTestDomains.join(","));
    // Create a new tab and load about:addons
    let blanktab = gBrowser.addTab();
    gBrowser.selectedTab = blanktab;
    BrowserOpenAddonsMgr('addons://list/service');

    is(blanktab, gBrowser.selectedTab, "Current tab should be blank tab");

    gBrowser.selectedBrowser.addEventListener("load", function tabLoad() {
      gBrowser.selectedBrowser.removeEventListener("load", tabLoad, true);
      let browser = blanktab.linkedBrowser;
      is(browser.currentURI.spec, "about:addons", "about:addons should load into blank tab.");

      addBuiltinManifest(gProviders[0]);
      activateOneProvider(gProviders[0], true, function() {
        gBrowser.removeTab(gBrowser.selectedTab);
        tabsToRemove.pop();
        // uninstall the provider
        clickAddonRemoveButton(blanktab, function(addon) {
          checkSocialUI();
          activateOneProvider(gProviders[0], true, function() {

            // after closing the addons tab, verify provider is still installed
            gBrowser.tabContainer.addEventListener("TabClose", function onTabClose() {
              gBrowser.tabContainer.removeEventListener("TabClose", onTabClose);
              AddonManager.getAddonsByTypes(["service"], function(aAddons) {
                is(aAddons.length, 1, "there can be only one");
                Services.prefs.clearUserPref("social.whitelist");
                next();
              });
            });

            // verify only one provider in list
            AddonManager.getAddonsByTypes(["service"], function(aAddons) {
              is(aAddons.length, 1, "there can be only one");

              let doc = blanktab.linkedBrowser.contentDocument;
              let list = doc.getElementById("addon-list");
              is(list.childNodes.length, 1, "only one addon is displayed");

              gBrowser.removeTab(blanktab);
            });

          });
        });
      });
    }, true);
  }
}
