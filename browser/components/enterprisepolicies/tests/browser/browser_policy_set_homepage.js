/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

ChromeUtils.defineModuleGetter(this, "HomePage",
                               "resource:///modules/HomePage.jsm");

registerCleanupFunction(function restore_pref_values() {
  // These two prefs are set as user prefs in case the "Locked"
  // option from this policy was not used. In this case, it won't
  // be tracked nor restored by the PoliciesPrefTracker.
  Services.prefs.clearUserPref("browser.startup.homepage");
  Services.prefs.clearUserPref("browser.startup.page");
});

async function check_homepage({expectedURL, expectedPageVal = 1, locked = false}) {
  is(HomePage.get(),
     expectedURL, "Homepage URL should match expected");
  is(Services.prefs.getIntPref("browser.startup.page", -1), expectedPageVal,
     "Pref page value should match expected");
  is(Services.prefs.prefIsLocked("browser.startup.homepage"), locked,
     "Lock status of browser.startup.homepage should match expected");
  is(Services.prefs.prefIsLocked("browser.startup.page"), locked,
     "Lock status of browser.startup.page should match expected");

  // Test that UI is disabled when the Locked property is enabled
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:preferences");
  await ContentTask.spawn(tab.linkedBrowser, {expectedURL, expectedPageVal, locked},
                          // eslint-disable-next-line no-shadow
                          async function({expectedURL, expectedPageVal, locked}) {
    let browserRestoreSessionCheckbox = content.document.getElementById("browserRestoreSession");
    is(browserRestoreSessionCheckbox.disabled, locked,
       "Disabled status of session restore status should match expected");
    let shouldBeChecked = expectedPageVal === 3;
    is(browserRestoreSessionCheckbox.checked, shouldBeChecked,
       "Session restore status checkbox should be: " + (shouldBeChecked ? "checked" : "unchecked"));

    content.document.getElementById("category-home").click();

    let homepageTextbox = content.document.getElementById("homePageUrl");
    // Unfortunately this test does not work because the new UI does not fill
    // default values into the URL box at the moment.
    // is(homepageTextbox.value, expectedURL,
    //    "Homepage URL should match expected");

    // Wait for rendering to be finished
    await ContentTaskUtils.waitForCondition(() => content.document.getElementById("useCurrentBtn").disabled === locked);

    is(homepageTextbox.disabled, locked,
       "Homepage URL text box disabled status should match expected");
    is(content.document.getElementById("homeMode").disabled, locked,
       "Home mode drop down disabled status should match expected");
    is(content.document.getElementById("useCurrentBtn").disabled, locked,
       "\"Use current page\" button disabled status should match expected");
    is(content.document.getElementById("useBookmarkBtn").disabled, locked,
      "\"Use bookmark\" button disabled status should match expected");
    is(content.document.getElementById("restoreDefaultHomePageBtn").disabled,
       locked, "\"Restore defaults\" button disabled status should match expected");
  });
  await BrowserTestUtils.removeTab(tab);
}

add_task(async function homepage_test_simple() {
  await setupPolicyEngineWithJson({
    "policies": {
      "Homepage": {
        "URL": "http://example1.com/"
      }
    }
  });
  await check_homepage({expectedURL: "http://example1.com/"});
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
  await check_homepage({expectedURL: "http://example2.com/",
                       expectedPageVal: 3});
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
  await check_homepage({expectedURL: "http://example3.com/"});
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
  await check_homepage({expectedURL: "http://example1.com/"});
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
  await check_homepage({expectedURL: "http://example1.com/|http://example2.com/"});
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
  await check_homepage({expectedURL: "http://example1.com/|http://example2.com/|http://example3.com/"});
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
  await check_homepage({expectedURL: "http://example4.com/|http://example5.com/|http://example6.com/",
                       locked: true});
});
