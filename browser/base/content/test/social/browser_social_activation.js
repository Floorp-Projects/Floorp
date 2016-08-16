/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

///////////////////
//
// Whitelisting this test.
// As part of bug 1077403, the leaking uncaught rejection should be fixed.
//
thisTestLeaksUncaughtRejectionsAndShouldBeFixed("TypeError: Assert is null");


var SocialService = Cu.import("resource://gre/modules/SocialService.jsm", {}).SocialService;

var tabsToRemove = [];

function removeProvider(provider) {
  return new Promise(resolve => {
    // a full install sets the manifest into a pref, addProvider alone doesn't,
    // make sure we uninstall if the manifest was added.
    if (provider.manifest) {
      SocialService.uninstallProvider(provider.origin, resolve);
    } else {
      SocialService.disableProvider(provider.origin, resolve);
    }
  });
}

function postTestCleanup(callback) {
  Task.spawn(function () {
    // any tabs opened by the test.
    for (let tab of tabsToRemove) {
      yield BrowserTestUtils.removeTab(tab);
    }
    tabsToRemove = [];
    // all the providers may have been added.
    while (Social.providers.length > 0) {
      yield removeProvider(Social.providers[0]);
    }
  }).then(callback);
}

function newTab(url) {
  return new Promise(resolve => {
    BrowserTestUtils.openNewForegroundTab(gBrowser, url).then(tab => {
      tabsToRemove.push(tab);
      resolve(tab);
    });
  });
}

function sendActivationEvent(tab, callback, nullManifest) {
  // hack Social.lastEventReceived so we don't hit the "too many events" check.
  Social.lastEventReceived = 0;
  BrowserTestUtils.synthesizeMouseAtCenter("#activation", {}, tab.linkedBrowser);
  executeSoon(callback);
}

function activateProvider(domain, callback, nullManifest) {
  let activationURL = domain+"/browser/browser/base/content/test/social/social_activate_basic.html"
  newTab(activationURL).then(tab => {
    sendActivationEvent(tab, callback, nullManifest);
  });
}

function activateIFrameProvider(domain, callback) {
  let activationURL = domain+"/browser/browser/base/content/test/social/social_activate_iframe.html"
  newTab(activationURL).then(tab => {
    sendActivationEvent(tab, callback, false);
  });
}

function waitForProviderLoad(origin) {
  return Promise.all([
    ensureFrameLoaded(gBrowser, origin + "/browser/browser/base/content/test/social/social_postActivation.html"),
  ]);
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

    let doc = tab.linkedBrowser.contentDocument;;
    let list = doc.getElementById("addon-list");

    let item = getAddonItemInList(addon.id, list);
    let button = item._removeBtn;
    isnot(button, null, "Should have a remove button");
    ok(!button.disabled, "Button should not be disabled");

    // uninstall happens after about:addons tab is closed, so we wait on
    // disabled
    promiseObserverNotified("social:provider-disabled").then(() => {
      is(item.getAttribute("pending"), "uninstall", "Add-on should be uninstalling");
      executeSoon(function() { aCallback(addon); });
    });

    BrowserTestUtils.synthesizeMouseAtCenter(button, {}, tab.linkedBrowser);
  });
}

function activateOneProvider(manifest, finishActivation, aCallback) {
  info("activating provider "+manifest.name);
  let panel = document.getElementById("servicesInstall-notification");
  BrowserTestUtils.waitForEvent(PopupNotifications.panel, "popupshown").then(() => {
    ok(!panel.hidden, "servicesInstall-notification panel opened");
    if (finishActivation)
      panel.button.click();
    else
      panel.closebutton.click();
  });
  BrowserTestUtils.waitForEvent(PopupNotifications.panel, "popuphidden").then(() => {
    ok(panel.hidden, "servicesInstall-notification panel hidden");
    if (!finishActivation) {
      ok(panel.hidden, "activation panel is not showing");
      executeSoon(aCallback);
    } else {
      waitForProviderLoad(manifest.origin).then(() => {
        checkSocialUI();
        executeSoon(aCallback);
      });
    }
  });

  // the test will continue as the popup events fire...
  activateProvider(manifest.origin, function() {
    info("waiting on activation panel to open/close...");
  });
}

var gTestDomains = ["https://example.com", "https://test1.example.com", "https://test2.example.com"];
var gProviders = [
  {
    name: "provider 1",
    origin: "https://example.com",
    shareURL: "https://example.com/browser/browser/base/content/test/social/social_share.html?provider1",
    iconURL: "chrome://branding/content/icon48.png"
  },
  {
    name: "provider 2",
    origin: "https://test1.example.com",
    shareURL: "https://test1.example.com/browser/browser/base/content/test/social/social_share.html?provider2",
    iconURL: "chrome://branding/content/icon64.png"
  },
  {
    name: "provider 3",
    origin: "https://test2.example.com",
    shareURL: "https://test2.example.com/browser/browser/base/content/test/social/social_share.html?provider2",
    iconURL: "chrome://branding/content/about-logo.png"
  }
];


function test() {
  PopupNotifications.panel.setAttribute("animate", "false");
  registerCleanupFunction(function () {
    PopupNotifications.panel.removeAttribute("animate");
  });
  waitForExplicitFinish();
  runSocialTests(tests, undefined, postTestCleanup);
}

var tests = {
  testActivationWrongOrigin: function(next) {
    // At this stage none of our providers exist, so we expect failure.
    Services.prefs.setBoolPref("social.remote-install.enabled", false);
    activateProvider(gTestDomains[0], function() {
      is(SocialUI.enabled, false, "SocialUI is not enabled");
      let panel = document.getElementById("servicesInstall-notification");
      ok(panel.hidden, "activation panel still hidden");
      checkSocialUI();
      Services.prefs.clearUserPref("social.remote-install.enabled");
      next();
    });
  },
  
  testIFrameActivation: function(next) {
    activateIFrameProvider(gTestDomains[0], function() {
      is(SocialUI.enabled, false, "SocialUI is not enabled");
      let panel = document.getElementById("servicesInstall-notification");
      ok(panel.hidden, "activation panel still hidden");
      checkSocialUI();
      next();
    });
  },
  
  testActivationFirstProvider: function(next) {
    // first up we add a manifest entry for a single provider.
    activateOneProvider(gProviders[0], false, function() {
      // we deactivated leaving no providers left, so Social is disabled.
      checkSocialUI();
      next();
    });
  },
  
  testActivationMultipleProvider: function(next) {
    // The trick with this test is to make sure that Social.providers[1] is
    // the current provider when doing the undo - this makes sure that the
    // Social code doesn't fallback to Social.providers[0], which it will
    // do in some cases (but those cases do not include what this test does)
    // first enable the 2 providers
    SocialService.addProvider(gProviders[0], function() {
      SocialService.addProvider(gProviders[1], function() {
        checkSocialUI();
        // activate the last provider.
        activateOneProvider(gProviders[2], false, function() {
          // we deactivated - the first provider should be enabled.
          checkSocialUI();
          next();
        });
      });
    });
  },

  testAddonManagerDoubleInstall: function(next) {
    // Create a new tab and load about:addons
    let addonsTab = gBrowser.addTab();
    gBrowser.selectedTab = addonsTab;
    BrowserOpenAddonsMgr('addons://list/service');
    gBrowser.selectedBrowser.addEventListener("load", function tabLoad() {
      gBrowser.selectedBrowser.removeEventListener("load", tabLoad, true);
      is(addonsTab.linkedBrowser.currentURI.spec, "about:addons", "about:addons should load into blank tab.");

      activateOneProvider(gProviders[0], true, function() {
        info("first activation completed");
        is(gBrowser.contentDocument.location.href, gProviders[0].origin + "/browser/browser/base/content/test/social/social_postActivation.html", "postActivationURL loaded");
        BrowserTestUtils.removeTab(gBrowser.selectedTab).then(() => {
          is(gBrowser.contentDocument.location.href, gProviders[0].origin + "/browser/browser/base/content/test/social/social_activate_basic.html", "activation page selected");
          BrowserTestUtils.removeTab(gBrowser.selectedTab).then(() => {
            tabsToRemove.pop();
            // uninstall the provider
            clickAddonRemoveButton(addonsTab, function(addon) {
              checkSocialUI();
              activateOneProvider(gProviders[0], true, function() {
                info("second activation completed");
                is(gBrowser.contentDocument.location.href, gProviders[0].origin + "/browser/browser/base/content/test/social/social_postActivation.html", "postActivationURL loaded");
                BrowserTestUtils.removeTab(gBrowser.selectedTab).then(() => {

                  // after closing the addons tab, verify provider is still installed
                  AddonManager.getAddonsByTypes(["service"], function(aAddons) {
                    is(aAddons.length, 1, "there can be only one");

                    let doc = addonsTab.linkedBrowser.contentDocument;
                    let list = doc.getElementById("addon-list");
                    is(list.childNodes.length, 1, "only one addon is displayed");

                    BrowserTestUtils.removeTab(addonsTab).then(next);
                  });
                });
              });
            });
          });
        });
      });
    }, true);
  }
}
