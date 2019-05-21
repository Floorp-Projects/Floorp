/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const {UrlClassifierTestUtils} = ChromeUtils.import("resource://testing-common/UrlClassifierTestUtils.jsm");
XPCOMUtils.defineLazyServiceGetter(Services, "cookies",
                                   "@mozilla.org/cookieService;1",
                                   "nsICookieService");
XPCOMUtils.defineLazyServiceGetter(Services, "cookiemgr",
                                   "@mozilla.org/cookiemanager;1",
                                   "nsICookieManager");

function restore_prefs() {
  Services.prefs.clearUserPref("network.cookie.cookieBehavior");
  Services.prefs.clearUserPref("network.cookie.lifetimePolicy");
  Services.prefs.clearUserPref("network.cookieSettings.unblocked_for_testing");
}

registerCleanupFunction(restore_prefs);

async function fake_profile_change() {
  await new Promise(resolve => {
    Services.obs.addObserver(function waitForDBClose() {
      Services.obs.removeObserver(waitForDBClose, "cookie-db-closed");
      resolve();
    }, "cookie-db-closed");
    Services.cookies.QueryInterface(Ci.nsIObserver).observe(null, "profile-before-change", "shutdown-persist");
  });
  await new Promise(resolve => {
    Services.obs.addObserver(function waitForDBOpen() {
      Services.obs.removeObserver(waitForDBOpen, "cookie-db-read");
      resolve();
    }, "cookie-db-read");
    Services.cookies.QueryInterface(Ci.nsIObserver).observe(null, "profile-do-change", "");
  });
}

async function test_cookie_settings({
                                      cookiesEnabled,
                                      thirdPartyCookiesEnabled,
                                      cookiesExpireAfterSession,
                                      rejectTrackers,
                                      cookieSettingsLocked,
                                    }) {
  let firstPartyURI = NetUtil.newURI("http://example.com/");
  let thirdPartyURI = NetUtil.newURI("http://example.org/");
  let channel = NetUtil.newChannel({uri: firstPartyURI,
                                    loadUsingSystemPrincipal: true});
  channel.QueryInterface(Ci.nsIHttpChannelInternal).forceAllowThirdPartyCookie = true;
  Services.cookies.removeAll();
  Services.cookies.setCookieString(firstPartyURI, null, "key=value", channel);
  Services.cookies.setCookieString(thirdPartyURI, null, "key=value", channel);

  let expectedFirstPartyCookies = 1;
  let expectedThirdPartyCookies = 1;
  if (!cookiesEnabled) {
    expectedFirstPartyCookies = 0;
  }
  if (!cookiesEnabled || !thirdPartyCookiesEnabled) {
    expectedThirdPartyCookies = 0;
  }
  is(Services.cookiemgr.countCookiesFromHost(firstPartyURI.host),
     expectedFirstPartyCookies,
     "Number of first-party cookies should match expected");
  is(Services.cookiemgr.countCookiesFromHost(thirdPartyURI.host),
     expectedThirdPartyCookies,
     "Number of third-party cookies should match expected");

  // Add a cookie so we can check if it persists past the end of the session
  // but, first remove existing cookies set by this host to put us in a known state
  Services.cookies.removeAll();
  Services.cookies.setCookieString(firstPartyURI, null, "key=value; max-age=1000", channel);

  await fake_profile_change();

  // Now check if the cookie persisted or not
  let expectedCookieCount = 1;
  if (cookiesExpireAfterSession || !cookiesEnabled) {
    expectedCookieCount = 0;
  }
  is(Services.cookies.countCookiesFromHost(firstPartyURI.host), expectedCookieCount,
     "Number of cookies was not what expected after restarting session");

  is(Services.prefs.prefIsLocked("network.cookie.cookieBehavior"), cookieSettingsLocked,
     "Cookie behavior pref lock status should be what is expected");
  is(Services.prefs.prefIsLocked("network.cookie.lifetimePolicy"), cookieSettingsLocked,
     "Cookie lifetime pref lock status should be what is expected");

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:preferences");
  await tab.linkedBrowser.contentWindow.gotoPref("panePrivacy");
  // eslint-disable-next-line no-shadow
  await ContentTask.spawn(tab.linkedBrowser, {cookiesEnabled, cookieSettingsLocked}, async function({cookiesEnabled, cookieSettingsLocked}) {
    let deleteOnCloseCheckbox = content.document.getElementById("deleteOnClose");
    isnot(deleteOnCloseCheckbox, null, "deleteOnCloseCheckbox should not be null.");

    let expectControlsDisabled = !cookiesEnabled || cookieSettingsLocked;
    is(deleteOnCloseCheckbox.disabled, expectControlsDisabled,
        "\"Delete cookies when Firefox is closed\" checkbox disabled status should match expected");
  });
  BrowserTestUtils.removeTab(tab);

  if (rejectTrackers) {
    tab = await BrowserTestUtils.addTab(gBrowser, "http://example.net/browser/browser/components/enterprisepolicies/tests/browser/page.html");
    let browser = gBrowser.getBrowserForTab(tab);
    await BrowserTestUtils.browserLoaded(browser);
    await ContentTask.spawn(tab.linkedBrowser, {}, async function() {
      // Load the script twice
      {
        let src = content.document.createElement("script");
        let p = new content.Promise((resolve, reject) => { src.onload = resolve; src.onerror = reject; });
        content.document.body.appendChild(src);
        src.src = "https://tracking.example.org/browser/browser/components/enterprisepolicies/tests/browser/subResources.sjs?what=script";
        await p;
      }
      {
        let src = content.document.createElement("script");
        let p = new content.Promise(resolve => { src.onload = resolve; });
        content.document.body.appendChild(src);
        src.src = "https://tracking.example.org/browser/browser/components/enterprisepolicies/tests/browser/subResources.sjs?what=script";
        await p;
      }
    });
    BrowserTestUtils.removeTab(tab);
    await fetch("https://tracking.example.org/browser/browser/components/enterprisepolicies/tests/browser/subResources.sjs?result&what=script")
      .then(r => r.text())
      .then(text => {
        is(text, 0, "\"Reject Tracker\" pref should match what is expected");
      });
  }
}

add_task(async function prepare_tracker_tables() {
  await UrlClassifierTestUtils.addTestTrackers();
});

add_task(async function test_initial_state() {
  Services.prefs.setBoolPref("network.cookieSettings.unblocked_for_testing", true);
  await test_cookie_settings({
    cookiesEnabled: true,
    thirdPartyCookiesEnabled: true,
    cookiesExpireAfterSession: false,
    cookieSettingsLocked: false,
  });
  restore_prefs();
});

add_task(async function test_undefined_unlocked() {
  Services.prefs.setIntPref("network.cookie.cookieBehavior", 3);
  Services.prefs.setIntPref("network.cookie.lifetimePolicy", 2);
  Services.prefs.setBoolPref("network.cookieSettings.unblocked_for_testing", true);
  await setupPolicyEngineWithJson({
    "policies": {
      "Cookies": {
      },
    },
  });
  is(Services.prefs.getIntPref("network.cookie.cookieBehavior", undefined), 3,
     "An empty cookie policy should not have changed the cookieBehavior preference");
  is(Services.prefs.getIntPref("network.cookie.lifetimePolicy", undefined), 2,
     "An empty cookie policy should not have changed the lifetimePolicy preference");
  restore_prefs();
});

add_task(async function test_disabled() {
  Services.prefs.setBoolPref("network.cookieSettings.unblocked_for_testing", true);
  await setupPolicyEngineWithJson({
    "policies": {
      "Cookies": {
        "Default": false,
      },
    },
  });

  await test_cookie_settings({
    cookiesEnabled: false,
    thirdPartyCookiesEnabled: true,
    cookiesExpireAfterSession: false,
    cookieSettingsLocked: false,
  });
  restore_prefs();
});

add_task(async function test_third_party_disabled() {
  Services.prefs.setBoolPref("network.cookieSettings.unblocked_for_testing", true);
  await setupPolicyEngineWithJson({
    "policies": {
      "Cookies": {
        "AcceptThirdParty": "never",
      },
    },
  });

  await test_cookie_settings({
    cookiesEnabled: true,
    thirdPartyCookiesEnabled: false,
    cookiesExpireAfterSession: false,
    cookieSettingsLocked: false,
  });
  restore_prefs();
});

add_task(async function test_disabled_and_third_party_disabled() {
  Services.prefs.setBoolPref("network.cookieSettings.unblocked_for_testing", true);
  await setupPolicyEngineWithJson({
    "policies": {
      "Cookies": {
        "Default": false,
        "AcceptThirdParty": "never",
      },
    },
  });

  await test_cookie_settings({
    cookiesEnabled: false,
    thirdPartyCookiesEnabled: false,
    cookiesExpireAfterSession: false,
    cookieSettingsLocked: false,
  });
  restore_prefs();
});

add_task(async function test_disabled_and_third_party_disabled_locked() {
  Services.prefs.setBoolPref("network.cookieSettings.unblocked_for_testing", true);
  await setupPolicyEngineWithJson({
    "policies": {
      "Cookies": {
        "Default": false,
        "AcceptThirdParty": "never",
        "Locked": true,
      },
    },
  });

  await test_cookie_settings({
    cookiesEnabled: false,
    thirdPartyCookiesEnabled: false,
    cookiesExpireAfterSession: false,
    cookieSettingsLocked: true,
  });
  restore_prefs();
});

add_task(async function test_undefined_locked() {
  Services.prefs.setBoolPref("network.cookieSettings.unblocked_for_testing", true);
  await setupPolicyEngineWithJson({
    "policies": {
      "Cookies": {
        "Locked": true,
      },
    },
  });

  await test_cookie_settings({
    cookiesEnabled: true,
    thirdPartyCookiesEnabled: true,
    cookiesExpireAfterSession: false,
    cookieSettingsLocked: true,
  });
  restore_prefs();
});

add_task(async function test_cookie_expire() {
  Services.prefs.setBoolPref("network.cookieSettings.unblocked_for_testing", true);
  await setupPolicyEngineWithJson({
    "policies": {
      "Cookies": {
        "ExpireAtSessionEnd": true,
      },
    },
  });

  await test_cookie_settings({
    cookiesEnabled: true,
    thirdPartyCookiesEnabled: true,
    cookiesExpireAfterSession: true,
    cookieSettingsLocked: false,
  });
  restore_prefs();
});

add_task(async function test_cookie_reject_trackers() {
  Services.prefs.setBoolPref("network.cookieSettings.unblocked_for_testing", true);
  await setupPolicyEngineWithJson({
    "policies": {
      "Cookies": {
        "RejectTracker": true,
      },
    },
  });

  await test_cookie_settings({
    cookiesEnabled: true,
    thirdPartyCookiesEnabled: true,
    cookiesExpireAfterSession: false,
    rejectTrackers: true,
    cookieSettingsLocked: false,
  });
  restore_prefs();
});

add_task(async function test_cookie_expire_locked() {
  Services.prefs.setBoolPref("network.cookieSettings.unblocked_for_testing", true);
  await setupPolicyEngineWithJson({
    "policies": {
      "Cookies": {
        "ExpireAtSessionEnd": true,
        "Locked": true,
      },
    },
  });

  await test_cookie_settings({
    cookiesEnabled: true,
    thirdPartyCookiesEnabled: true,
    cookiesExpireAfterSession: true,
    cookieSettingsLocked: true,
  });
  restore_prefs();
});

add_task(async function test_disabled_cookie_expire_locked() {
  Services.prefs.setBoolPref("network.cookieSettings.unblocked_for_testing", true);
  await setupPolicyEngineWithJson({
    "policies": {
      "Cookies": {
        "Default": false,
        "AcceptThirdParty": "never",
        "ExpireAtSessionEnd": true,
        "Locked": true,
      },
    },
  });

  await test_cookie_settings({
    cookiesEnabled: false,
    thirdPartyCookiesEnabled: false,
    cookiesExpireAfterSession: true,
    cookieSettingsLocked: true,
  });
  restore_prefs();
});

add_task(async function prepare_tracker_tables() {
  await UrlClassifierTestUtils.cleanupTestTrackers();
});
