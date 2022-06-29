/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const { UrlClassifierTestUtils } = ChromeUtils.import(
  "resource://testing-common/UrlClassifierTestUtils.jsm"
);
Services.cookies.QueryInterface(Ci.nsICookieService);

function restore_prefs() {
  // Bug 1617611: Fix all the tests broken by "cookies SameSite=lax by default"
  Services.prefs.clearUserPref("network.cookie.sameSite.laxByDefault");
  Services.prefs.clearUserPref("network.cookie.cookieBehavior");
  Services.prefs.clearUserPref(
    "network.cookieJarSettings.unblocked_for_testing"
  );
  Services.prefs.clearUserPref(
    "network.cookie.rejectForeignWithExceptions.enabled"
  );
}

registerCleanupFunction(restore_prefs);

async function fake_profile_change() {
  await new Promise(resolve => {
    Services.obs.addObserver(function waitForDBClose() {
      Services.obs.removeObserver(waitForDBClose, "cookie-db-closed");
      resolve();
    }, "cookie-db-closed");
    Services.cookies
      .QueryInterface(Ci.nsIObserver)
      .observe(null, "profile-before-change", null);
  });
  await new Promise(resolve => {
    Services.obs.addObserver(function waitForDBOpen() {
      Services.obs.removeObserver(waitForDBOpen, "cookie-db-read");
      resolve();
    }, "cookie-db-read");
    Services.cookies
      .QueryInterface(Ci.nsIObserver)
      .observe(null, "profile-do-change", "");
  });
}

async function test_cookie_settings({
  cookiesEnabled,
  thirdPartyCookiesEnabled,
  rejectTrackers,
  cookieJarSettingsLocked,
}) {
  let firstPartyURI = NetUtil.newURI("http://example.com/");
  let thirdPartyURI = NetUtil.newURI("http://example.org/");
  let channel = NetUtil.newChannel({
    uri: firstPartyURI,
    loadUsingSystemPrincipal: true,
  });
  channel.QueryInterface(
    Ci.nsIHttpChannelInternal
  ).forceAllowThirdPartyCookie = true;
  Services.cookies.removeAll();
  Services.cookies.setCookieStringFromHttp(firstPartyURI, "key=value", channel);
  Services.cookies.setCookieStringFromHttp(thirdPartyURI, "key=value", channel);

  let expectedFirstPartyCookies = 1;
  let expectedThirdPartyCookies = 1;
  if (!cookiesEnabled) {
    expectedFirstPartyCookies = 0;
  }
  if (!cookiesEnabled || !thirdPartyCookiesEnabled) {
    expectedThirdPartyCookies = 0;
  }
  is(
    Services.cookies.countCookiesFromHost(firstPartyURI.host),
    expectedFirstPartyCookies,
    "Number of first-party cookies should match expected"
  );
  is(
    Services.cookies.countCookiesFromHost(thirdPartyURI.host),
    expectedThirdPartyCookies,
    "Number of third-party cookies should match expected"
  );

  // Add a cookie so we can check if it persists past the end of the session
  // but, first remove existing cookies set by this host to put us in a known state
  Services.cookies.removeAll();
  Services.cookies.setCookieStringFromHttp(
    firstPartyURI,
    "key=value; max-age=1000",
    channel
  );

  await fake_profile_change();

  // Now check if the cookie persisted or not
  let expectedCookieCount = 1;
  if (!cookiesEnabled) {
    expectedCookieCount = 0;
  }
  is(
    Services.cookies.countCookiesFromHost(firstPartyURI.host),
    expectedCookieCount,
    "Number of cookies was not what expected after restarting session"
  );

  is(
    Services.prefs.prefIsLocked("network.cookie.cookieBehavior"),
    cookieJarSettingsLocked,
    "Cookie behavior pref lock status should be what is expected"
  );

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:preferences"
  );

  BrowserTestUtils.removeTab(tab);

  if (rejectTrackers) {
    tab = await BrowserTestUtils.addTab(
      gBrowser,
      "http://example.net/browser/browser/components/enterprisepolicies/tests/browser/page.html"
    );
    let browser = gBrowser.getBrowserForTab(tab);
    await BrowserTestUtils.browserLoaded(browser);
    await SpecialPowers.spawn(tab.linkedBrowser, [], async function() {
      // Load the script twice
      {
        let src = content.document.createElement("script");
        let p = new content.Promise((resolve, reject) => {
          src.onload = resolve;
          src.onerror = reject;
        });
        content.document.body.appendChild(src);
        src.src =
          "https://tracking.example.org/browser/browser/components/enterprisepolicies/tests/browser/subResources.sjs?what=script";
        await p;
      }
      {
        let src = content.document.createElement("script");
        let p = new content.Promise(resolve => {
          src.onload = resolve;
        });
        content.document.body.appendChild(src);
        src.src =
          "https://tracking.example.org/browser/browser/components/enterprisepolicies/tests/browser/subResources.sjs?what=script";
        await p;
      }
    });
    BrowserTestUtils.removeTab(tab);
    await fetch(
      "https://tracking.example.org/browser/browser/components/enterprisepolicies/tests/browser/subResources.sjs?result&what=script"
    )
      .then(r => r.text())
      .then(text => {
        is(text, "0", '"Reject Tracker" pref should match what is expected');
      });
  }
}

add_task(async function prepare_tracker_tables() {
  await UrlClassifierTestUtils.addTestTrackers();
});

add_task(async function test_initial_state() {
  Services.prefs.setBoolPref(
    "network.cookieJarSettings.unblocked_for_testing",
    true
  );
  Services.prefs.setBoolPref(
    "network.cookie.rejectForeignWithExceptions.enabled",
    false
  );
  Services.prefs.setBoolPref("network.cookie.sameSite.laxByDefault", false);

  await test_cookie_settings({
    cookiesEnabled: true,
    thirdPartyCookiesEnabled: true,
    cookieJarSettingsLocked: false,
  });
  restore_prefs();
});

add_task(async function test_undefined_unlocked() {
  Services.prefs.setIntPref("network.cookie.cookieBehavior", 3);
  Services.prefs.setBoolPref(
    "network.cookieJarSettings.unblocked_for_testing",
    true
  );
  Services.prefs.setBoolPref(
    "network.cookie.rejectForeignWithExceptions.enabled",
    false
  );
  await setupPolicyEngineWithJson({
    policies: {
      Cookies: {},
    },
  });
  is(
    Services.prefs.getIntPref("network.cookie.cookieBehavior", undefined),
    3,
    "An empty cookie policy should not have changed the cookieBehavior preference"
  );
  restore_prefs();
});

add_task(async function test_disabled() {
  Services.prefs.setBoolPref(
    "network.cookieJarSettings.unblocked_for_testing",
    true
  );
  Services.prefs.setBoolPref(
    "network.cookie.rejectForeignWithExceptions.enabled",
    false
  );
  await setupPolicyEngineWithJson({
    policies: {
      Cookies: {
        Default: false,
      },
    },
  });

  await test_cookie_settings({
    cookiesEnabled: false,
    thirdPartyCookiesEnabled: true,
    cookieJarSettingsLocked: false,
  });
  restore_prefs();
});

add_task(async function test_third_party_disabled() {
  Services.prefs.setBoolPref(
    "network.cookieJarSettings.unblocked_for_testing",
    true
  );
  Services.prefs.setBoolPref(
    "network.cookie.rejectForeignWithExceptions.enabled",
    false
  );
  await setupPolicyEngineWithJson({
    policies: {
      Cookies: {
        AcceptThirdParty: "never",
      },
    },
  });

  await test_cookie_settings({
    cookiesEnabled: true,
    thirdPartyCookiesEnabled: false,
    cookieJarSettingsLocked: false,
  });
  restore_prefs();
});

add_task(async function test_disabled_and_third_party_disabled() {
  Services.prefs.setBoolPref(
    "network.cookieJarSettings.unblocked_for_testing",
    true
  );
  Services.prefs.setBoolPref(
    "network.cookie.rejectForeignWithExceptions.enabled",
    false
  );
  await setupPolicyEngineWithJson({
    policies: {
      Cookies: {
        Default: false,
        AcceptThirdParty: "never",
      },
    },
  });

  await test_cookie_settings({
    cookiesEnabled: false,
    thirdPartyCookiesEnabled: false,
    cookieJarSettingsLocked: false,
  });
  restore_prefs();
});

add_task(async function test_disabled_and_third_party_disabled_locked() {
  Services.prefs.setBoolPref(
    "network.cookieJarSettings.unblocked_for_testing",
    true
  );
  Services.prefs.setBoolPref(
    "network.cookie.rejectForeignWithExceptions.enabled",
    false
  );
  await setupPolicyEngineWithJson({
    policies: {
      Cookies: {
        Default: false,
        AcceptThirdParty: "never",
        Locked: true,
      },
    },
  });

  await test_cookie_settings({
    cookiesEnabled: false,
    thirdPartyCookiesEnabled: false,
    cookieJarSettingsLocked: true,
  });
  restore_prefs();
});

add_task(async function test_undefined_locked() {
  Services.prefs.setBoolPref(
    "network.cookieJarSettings.unblocked_for_testing",
    true
  );
  Services.prefs.setBoolPref(
    "network.cookie.rejectForeignWithExceptions.enabled",
    false
  );
  Services.prefs.setBoolPref("network.cookie.sameSite.laxByDefault", false);
  await setupPolicyEngineWithJson({
    policies: {
      Cookies: {
        Locked: true,
      },
    },
  });

  await test_cookie_settings({
    cookiesEnabled: true,
    thirdPartyCookiesEnabled: true,
    cookieJarSettingsLocked: true,
  });
  restore_prefs();
});

add_task(async function test_cookie_reject_trackers() {
  Services.prefs.setBoolPref(
    "network.cookieJarSettings.unblocked_for_testing",
    true
  );
  Services.prefs.setBoolPref(
    "network.cookie.rejectForeignWithExceptions.enabled",
    false
  );
  Services.prefs.setBoolPref("network.cookie.sameSite.laxByDefault", false);
  await setupPolicyEngineWithJson({
    policies: {
      Cookies: {
        RejectTracker: true,
      },
    },
  });

  await test_cookie_settings({
    cookiesEnabled: true,
    thirdPartyCookiesEnabled: true,
    rejectTrackers: true,
    cookieJarSettingsLocked: false,
  });
  restore_prefs();
});

add_task(async function prepare_tracker_tables() {
  await UrlClassifierTestUtils.cleanupTestTrackers();
});
