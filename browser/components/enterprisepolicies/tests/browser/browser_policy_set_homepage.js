/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

registerCleanupFunction(function restore_pref_values() {
  // These two prefs are set as user prefs in case the "Locked"
  // option from this policy was not used. In this case, it won't
  // be tracked nor restored by the PoliciesPrefTracker.
  Services.prefs.clearUserPref("browser.startup.homepage");
  Services.prefs.clearUserPref("browser.startup.page");
});

add_task(async function homepage_test_simple() {
  await setupPolicyEngineWithJson({
    "policies": {
      "Homepage": {
        "URL": "http://example1.com/"
      }
    }
  });
  is(Services.prefs.getStringPref("browser.startup.homepage", ""),
     "http://example1.com/", "Homepage URL should have been set");
  is(Services.prefs.getIntPref("browser.startup.page", -1), 1,
     "Homepage should be used instead of blank page or previous tabs");
});

add_task(async function homepage_test_repeat_same_policy_value() {
  // Simulate homepage change after policy applied
  Services.prefs.setStringPref("browser.startup.homepage",
                               "http://example2.com/");
  Services.prefs.setIntPref("browser.startup.page", 3);

  // Policy should have no effect. Homepage has not been locked and policy value
  // has not changed. We should be respecting the homepage that the user gave.
  await setupPolicyEngineWithJson({
    "policies": {
      "Homepage": {
        "URL": "http://example1.com/"
      }
    }
  });
  is(Services.prefs.getStringPref("browser.startup.homepage", ""),
     "http://example2.com/",
     "Homepage URL should not have been overridden by pre-existing policy value");
  is(Services.prefs.getIntPref("browser.startup.page", -1), 3,
     "Start page type should not have been overridden by pre-existing policy value");
});

add_task(async function homepage_test_different_policy_value() {
  // This policy is a change from the policy's previous value. This should
  // override the user's homepage
  await setupPolicyEngineWithJson({
    "policies": {
      "Homepage": {
        "URL": "http://example3.com/"
      }
    }
  });
  is(Services.prefs.getStringPref("browser.startup.homepage", ""),
     "http://example3.com/",
     "Homepage URL should have been overridden by the policy value");
  is(Services.prefs.getIntPref("browser.startup.page", -1), 1,
     "Start page type should have been overridden by setting the policy value");
});

add_task(async function homepage_test_empty_additional() {
  await setupPolicyEngineWithJson({
    "policies": {
      "Homepage": {
        "URL": "http://example1.com/",
        "Additional": []
      }
    }
  });
  is(Services.prefs.getStringPref("browser.startup.homepage", ""),
     "http://example1.com/", "Homepage URL should have been set properly");
});

add_task(async function homepage_test_single_additional() {
  await setupPolicyEngineWithJson({
    "policies": {
      "Homepage": {
        "URL": "http://example1.com/",
        "Additional": ["http://example2.com/"]
      }
    }
  });
  is(Services.prefs.getStringPref("browser.startup.homepage", ""),
     "http://example1.com/|http://example2.com/",
     "Homepage URL should have been set properly");

});

add_task(async function homepage_test_multiple_additional() {
  await setupPolicyEngineWithJson({
    "policies": {
      "Homepage": {
        "URL": "http://example1.com/",
        "Additional": ["http://example2.com/",
                       "http://example3.com/"]
      }
    }
  });
  is(Services.prefs.getStringPref("browser.startup.homepage", ""),
     "http://example1.com/|http://example2.com/|http://example3.com/",
     "Homepage URL should have been set properly");

});

add_task(async function homepage_test_locked() {
  await setupPolicyEngineWithJson({
    "policies": {
      "Homepage": {
        "URL": "http://example4.com/",
        "Additional": ["http://example5.com/",
                       "http://example6.com/"],
        "Locked": true
      }
    }
  });
  is(Services.prefs.getStringPref("browser.startup.homepage", ""),
     "http://example4.com/|http://example5.com/|http://example6.com/",
     "Homepage URL should have been set properly");
  is(Services.prefs.getIntPref("browser.startup.page", -1), 1,
     "Homepage should be used instead of blank page or previous tabs");
  is(Services.prefs.prefIsLocked("browser.startup.homepage"), true,
     "Homepage pref should be locked");
  is(Services.prefs.prefIsLocked("browser.startup.page"), true,
     "Start page type pref should be locked");

  // Test that UI is disabled when the Locked property is enabled
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:preferences");
  await ContentTask.spawn(tab.linkedBrowser, null, async function() {
    is(content.document.getElementById("browserStartupPage").disabled, true,
       "When homepage is locked, the startup page choice controls should be locked");
  });
  await BrowserTestUtils.loadURI(tab.linkedBrowser, "about:preferences#home");
  await ContentTask.spawn(tab.linkedBrowser, null, async function() {
    is(content.document.getElementById("homeMode").disabled, true,
       "Homepage menulist should be disabled");
    is(content.document.getElementById("homePageUrl").disabled, true,
       "Homepage custom input should be disabled");
    is(content.document.getElementById("useCurrentBtn").disabled, true,
       "\"Use Current Page\" button should be disabled");
    is(content.document.getElementById("useBookmarkBtn").disabled, true,
       "\"Use Bookmark...\" button should be disabled");
    is(content.document.getElementById("restoreDefaultHomePageBtn").disabled, true,
       "\"Restore to Default\" button should be disabled");
  });
  BrowserTestUtils.removeTab(tab);
});
