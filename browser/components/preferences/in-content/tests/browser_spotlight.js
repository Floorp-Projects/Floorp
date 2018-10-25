
add_task(async function test_reports_section() {
  let prefs = await openPreferencesViaOpenPreferencesAPI("privacy-reports", {leaveOpen: true});
  is(prefs.selectedPane, "panePrivacy", "Privacy pane is selected by default");
  let doc = gBrowser.contentDocument;
  is(doc.location.hash, "#privacy", "The subcategory should be removed from the URI");
  await TestUtils.waitForCondition(() => doc.querySelector(".spotlight"),
    "Wait for the reports section is spotlighted.");
  is(doc.querySelector(".spotlight").getAttribute("data-subcategory"), "reports",
    "The reports section is spotlighted.");
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function test_address_autofill_section() {
  let prefs = await openPreferencesViaOpenPreferencesAPI("privacy-address-autofill", {leaveOpen: true});
  is(prefs.selectedPane, "panePrivacy", "Privacy pane is selected by default");
  let doc = gBrowser.contentDocument;
  is(doc.location.hash, "#privacy", "The subcategory should be removed from the URI");
  await TestUtils.waitForCondition(() => doc.querySelector(".spotlight"),
    "Wait for the address-autofill section is spotlighted.");
  is(doc.querySelector(".spotlight").getAttribute("data-subcategory"), "address-autofill",
    "The address-autofill section is spotlighted.");
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function test_credit_card_autofill_section() {
  if (!Services.prefs.getBoolPref("extensions.formautofill.creditCards.available")) {
    return;
  }
  let prefs = await openPreferencesViaOpenPreferencesAPI("privacy-credit-card-autofill", {leaveOpen: true});
  is(prefs.selectedPane, "panePrivacy", "Privacy pane is selected by default");
  let doc = gBrowser.contentDocument;
  is(doc.location.hash, "#privacy", "The subcategory should be removed from the URI");
  await TestUtils.waitForCondition(() => doc.querySelector(".spotlight"),
    "Wait for the credit-card-autofill section is spotlighted.");
  is(doc.querySelector(".spotlight").getAttribute("data-subcategory"), "credit-card-autofill",
    "The credit-card-autofill section is spotlighted.");
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function test_form_autofill_section() {
  let prefs = await openPreferencesViaOpenPreferencesAPI("privacy-form-autofill", {leaveOpen: true});
  is(prefs.selectedPane, "panePrivacy", "Privacy pane is selected by default");
  let doc = gBrowser.contentDocument;
  is(doc.location.hash, "#privacy", "The subcategory should be removed from the URI");
  await TestUtils.waitForCondition(() => doc.querySelector(".spotlight"),
    "Wait for the form-autofill section is spotlighted.");
  is(doc.querySelector(".spotlight").getAttribute("data-subcategory"), "form-autofill",
    "The form-autofill section is spotlighted.");
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function test_change_cookie_settings() {
  await SpecialPowers.pushPrefEnv({set: [
    ["browser.contentblocking.enabled", true],
    ["browser.contentblocking.ui.enabled", true],
    ["browser.contentblocking.fastblock.ui.enabled", true],
    ["browser.contentblocking.trackingprotection.ui.enabled", true],
    ["browser.contentblocking.rejecttrackers.ui.enabled", true],
  ]});
  let prefs = await openPreferencesViaOpenPreferencesAPI("privacy-trackingprotection", {leaveOpen: true});
  is(prefs.selectedPane, "panePrivacy", "Privacy pane is selected by default");
  let doc = gBrowser.contentDocument;
  is(doc.location.hash, "#privacy", "The subcategory should be removed from the URI");
  await TestUtils.waitForCondition(() => doc.querySelector(".spotlight"),
    "Wait for the content-blocking section to be spotlighted.");
  is(doc.querySelector(".spotlight").getAttribute("data-subcategory"), "trackingprotection",
    "The tracking-protection section is spotlighted.");
  doc.defaultView.spotlight(null);
  is(doc.querySelector(".spotlight"), null,
    "The spotlighted section is cleared.");

  let changeCookieSettings = doc.getElementById("contentBlockingChangeCookieSettings");
  changeCookieSettings.doCommand();
  await TestUtils.waitForCondition(() => doc.querySelector(".spotlight"),
    "Wait for the content-blocking section to be spotlighted.");
  is(doc.querySelector(".spotlight").getAttribute("data-subcategory"), "sitedata",
    "The sitedata section is spotlighted.");
  is(prefs.selectedPane, "panePrivacy", "Privacy pane is selected by default");
  is(doc.location.hash, "#privacy", "The subcategory should be removed from the URI");

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
