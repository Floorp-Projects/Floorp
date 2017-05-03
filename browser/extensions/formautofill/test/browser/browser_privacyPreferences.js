"use strict";

const PAGE_PREFS = "about:preferences";
const PAGE_PRIVACY = PAGE_PREFS + "#privacy";
const GROUP_AUTOFILL = "#formAutofillGroup";
const CHECKBOX_AUTOFILL = GROUP_AUTOFILL + " checkbox";
const PREF_AUTOFILL_ENABLED = "browser.formautofill.enabled";
const TEST_SELECTORS = {
  group: GROUP_AUTOFILL,
  checkbox: CHECKBOX_AUTOFILL,
};

// Visibility of form autofill group should be hidden when opening
// preferences page.
add_task(function* test_aboutPreferences() {
  yield BrowserTestUtils.withNewTab({gBrowser, url: PAGE_PREFS}, function* (browser) {
    yield ContentTask.spawn(browser, TEST_SELECTORS, (args) => {
      is(content.document.querySelector(args.group).hidden, true,
        "Form Autofill group should be hidden");
    });
  });
});

// Visibility of form autofill group should be visible when opening
// directly to privacy page. Checkbox is checked by default.
add_task(function* test_aboutPreferencesPrivacy() {
  yield BrowserTestUtils.withNewTab({gBrowser, url: PAGE_PRIVACY}, function* (browser) {
    yield ContentTask.spawn(browser, TEST_SELECTORS, (args) => {
      is(content.document.querySelector(args.group).hidden, false,
        "Form Autofill group should be visible");
      is(content.document.querySelector(args.checkbox).checked, true,
        "Checkbox should be checked");
    });
  });
});

// Checkbox should be unchecked when form autofill is disabled.
add_task(function* test_autofillDisabledCheckbox() {
  SpecialPowers.pushPrefEnv({set: [[PREF_AUTOFILL_ENABLED, false]]});

  yield BrowserTestUtils.withNewTab({gBrowser, url: PAGE_PRIVACY}, function* (browser) {
    yield ContentTask.spawn(browser, TEST_SELECTORS, (args) => {
      is(content.document.querySelector(args.group).hidden, false,
        "Form Autofill group should be visible");
      is(content.document.querySelector(args.checkbox).checked, false,
        "Checkbox should be unchecked when Form Autofill is disabled");
    });
  });
});
