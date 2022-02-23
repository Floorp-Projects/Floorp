add_task(async function test_openPreferences_spotlight() {
  for (let [arg, expectedPane, expectedHash, expectedSubcategory] of [
    ["privacy-reports", "panePrivacy", "#privacy", "reports"],
    ["privacy-address-autofill", "panePrivacy", "#privacy", "address-autofill"],
    [
      "privacy-credit-card-autofill",
      "panePrivacy",
      "#privacy",
      "credit-card-autofill",
    ],
    ["privacy-form-autofill", "panePrivacy", "#privacy", "form-autofill"],
    ["privacy-logins", "panePrivacy", "#privacy", "logins"],
    [
      "privacy-trackingprotection",
      "panePrivacy",
      "#privacy",
      "trackingprotection",
    ],
    [
      "privacy-permissions-block-popups",
      "panePrivacy",
      "#privacy",
      "permissions-block-popups",
    ],
  ]) {
    if (
      arg == "privacy-credit-card-autofill" &&
      !Services.prefs.getBoolPref(
        "extensions.formautofill.creditCards.available"
      )
    ) {
      continue;
    }

    let prefs = await openPreferencesViaOpenPreferencesAPI(arg, {
      leaveOpen: true,
    });
    is(prefs.selectedPane, expectedPane, "The right pane is selected");
    let doc = gBrowser.contentDocument;
    is(
      doc.location.hash,
      expectedHash,
      "The subcategory should be removed from the URI"
    );
    await TestUtils.waitForCondition(
      () => doc.querySelector(".spotlight"),
      "Wait for the spotlight"
    );
    is(
      doc.querySelector(".spotlight").getAttribute("data-subcategory"),
      expectedSubcategory,
      "The right subcategory is spotlighted"
    );

    doc.defaultView.spotlight(null);
    is(
      doc.querySelector(".spotlight"),
      null,
      "The spotlighted section is cleared"
    );

    BrowserTestUtils.removeTab(gBrowser.selectedTab);
  }
});
