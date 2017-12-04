/* eslint-disable mozilla/no-cpows-in-tests */

add_task(async function test_reports_section() {
  let prefs = await openPreferencesViaOpenPreferencesAPI("privacy-reports", {leaveOpen: true});
  is(prefs.selectedPane, "panePrivacy", "Privacy pane is selected by default");
  let doc = gBrowser.contentDocument;
  is(doc.location.hash, "#privacy", "The subcategory should be removed from the URI");
  await TestUtils.waitForCondition(() => doc.querySelector(".spotlight"),
    "Wait for the reports section is spotlighted.");
  is(doc.querySelector(".spotlight").getAttribute("data-subcategory"), "reports",
    "The reports section is spotlighted.");
  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function test_address_autofill_section() {
  let prefs = await openPreferencesViaOpenPreferencesAPI("privacy-address-autofill", {leaveOpen: true});
  is(prefs.selectedPane, "panePrivacy", "Privacy pane is selected by default");
  let doc = gBrowser.contentDocument;
  is(doc.location.hash, "#privacy", "The subcategory should be removed from the URI");
  await TestUtils.waitForCondition(() => doc.querySelector(".spotlight"),
    "Wait for the ddress-autofill section is spotlighted.");
  is(doc.querySelector(".spotlight").getAttribute("data-subcategory"), "address-autofill",
    "The ddress-autofill section is spotlighted.");
  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function test_credit_card_autofill_section() {
  let prefs = await openPreferencesViaOpenPreferencesAPI("privacy-credit-card-autofill", {leaveOpen: true});
  is(prefs.selectedPane, "panePrivacy", "Privacy pane is selected by default");
  let doc = gBrowser.contentDocument;
  is(doc.location.hash, "#privacy", "The subcategory should be removed from the URI");
  await TestUtils.waitForCondition(() => doc.querySelector(".spotlight"),
    "Wait for the credit-card-autofill section is spotlighted.");
  is(doc.querySelector(".spotlight").getAttribute("data-subcategory"), "credit-card-autofill",
    "The credit-card-autofill section is spotlighted.");
  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
