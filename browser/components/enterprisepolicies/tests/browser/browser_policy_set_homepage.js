/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Prefs that will be touched by the test and need to be reset when the test
// cleans up.
const TOUCHED_PREFS = {
  "browser.startup.homepage": "String",
  "browser.startup.page": "Int",
  "pref.browser.homepage.disable_button.current_page": "Bool",
  "pref.browser.homepage.disable_button.bookmark_page": "Bool",
  "pref.browser.homepage.disable_button.restore_default": "Bool",
  "browser.policies.runOncePerModification.setHomepage": "String",
};
let ORIGINAL_PREF_VALUE = {};

add_task(function read_original_pref_values() {
  for (let pref in TOUCHED_PREFS) {
    let prefType = TOUCHED_PREFS[pref];
    ORIGINAL_PREF_VALUE[pref] =
      Services.prefs[`get${prefType}Pref`](pref, undefined);
  }
});
registerCleanupFunction(function restore_pref_values() {
  let defaults = Services.prefs.getDefaultBranch("");
  for (let pref in TOUCHED_PREFS) {
    Services.prefs.unlockPref(pref);
    Services.prefs.clearUserPref(pref);
    let originalValue = ORIGINAL_PREF_VALUE[pref];
    let prefType = TOUCHED_PREFS[pref];
    if (originalValue !== undefined) {
      defaults[`set${prefType}Pref`](pref, originalValue);
    }
  }
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
    is(content.document.getElementById("browserHomePage").disabled, true,
       "Homepage input should be disabled");
    is(content.document.getElementById("useCurrent").disabled, true,
       "\"Use Current Page\" button should be disabled");
    is(content.document.getElementById("useBookmark").disabled, true,
       "\"Use Bookmark...\" button should be disabled");
    is(content.document.getElementById("restoreDefaultHomePage").disabled, true,
       "\"Restore to Default\" button should be disabled");
  });
  BrowserTestUtils.removeTab(tab);
});
