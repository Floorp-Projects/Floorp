"use strict";

add_task(async function test_first_time_save() {
  let addresses = await getAddresses();
  is(addresses.length, 0, "No address in storage");
  await SpecialPowers.pushPrefEnv({
    set: [
      [FTU_PREF, true],
      [ENABLED_AUTOFILL_ADDRESSES_PREF, true],
      [AUTOFILL_ADDRESSES_AVAILABLE_PREF, "on"],
      [ENABLED_AUTOFILL_ADDRESSES_CAPTURE_PREF, true],
    ],
  });

  let onAdded = waitForStorageChangedEvents("add");
  await BrowserTestUtils.withNewTab({ gBrowser, url: FORM_URL }, async function(
    browser
  ) {
    let onPopupShown = waitForPopupShown();
    let tabPromise = BrowserTestUtils.waitForNewTab(
      gBrowser,
      "about:preferences#privacy"
    );
    await focusUpdateSubmitForm(browser, {
      focusSelector: "#organization",
      newValues: {
        "#organization": "Sesame Street",
        "#street-address": "123 Sesame Street",
        "#tel": "1-345-345-3456",
      },
    });

    await onPopupShown;
    let cb = getDoorhangerCheckbox();
    ok(cb.hidden, "Sync checkbox should be hidden");
    // Open the panel via main button
    await clickDoorhangerButton(MAIN_BUTTON);
    let tab = await tabPromise;
    ok(tab, "Privacy panel opened");
    BrowserTestUtils.removeTab(tab);
  });
  await onAdded;

  addresses = await getAddresses();
  is(addresses.length, 1, "Address saved");
  let ftuPref = SpecialPowers.getBoolPref(FTU_PREF);
  is(ftuPref, false, "First time use flag is false");
});

add_task(async function test_non_first_time_save() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [ENABLED_AUTOFILL_ADDRESSES_PREF, true],
      [AUTOFILL_ADDRESSES_AVAILABLE_PREF, "on"],
      [ENABLED_AUTOFILL_ADDRESSES_CAPTURE_PREF, true],
    ],
  });
  let addresses = await getAddresses();
  let ftuPref = SpecialPowers.getBoolPref(FTU_PREF);
  is(ftuPref, false, "First time use flag is false");
  is(addresses.length, 1, "1 address in storage");

  let onAdded = waitForStorageChangedEvents("add");
  await BrowserTestUtils.withNewTab({ gBrowser, url: FORM_URL }, async function(
    browser
  ) {
    await focusUpdateSubmitForm(browser, {
      focusSelector: "#organization",
      newValues: {
        "#organization": "Mozilla",
        "#street-address": "331 E. Evelyn Avenue",
        "#tel": "1-650-903-0800",
      },
    });

    await sleep(1000);
    is(PopupNotifications.panel.state, "closed", "Doorhanger is hidden");
  });
  await onAdded;

  addresses = await getAddresses();
  is(addresses.length, 2, "Another address saved");

  await SpecialPowers.popPrefEnv();
});

add_task(async function test_first_time_save_with_sync_account() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [FTU_PREF, true],
      [ENABLED_AUTOFILL_ADDRESSES_PREF, true],
      [AUTOFILL_ADDRESSES_AVAILABLE_PREF, "on"],
      [SYNC_USERNAME_PREF, "foo@bar.com"],
      [SYNC_ADDRESSES_PREF, false],
    ],
  });

  let onAdded = waitForStorageChangedEvents("add");
  await BrowserTestUtils.withNewTab({ gBrowser, url: FORM_URL }, async function(
    browser
  ) {
    let onPopupShown = waitForPopupShown();
    let tabPromise = BrowserTestUtils.waitForNewTab(
      gBrowser,
      "about:preferences#privacy-address-autofill"
    );
    await focusUpdateSubmitForm(browser, {
      focusSelector: "#organization",
      newValues: {
        "#organization": "Foobar",
        "#email": "foo@bar.com",
        "#tel": "1-234-567-8900",
      },
    });

    await onPopupShown;
    let cb = getDoorhangerCheckbox();
    ok(!cb.hidden, "Sync checkbox should be visible");

    is(cb.checked, false, "Checkbox state should match addresses sync state");
    cb.click();
    is(
      SpecialPowers.getBoolPref(SYNC_ADDRESSES_PREF),
      true,
      "addresses sync should be enabled after checked"
    );
    // Open the panel via main button
    await clickDoorhangerButton(MAIN_BUTTON);
    let tab = await tabPromise;
    ok(tab, "Privacy panel opened");
    BrowserTestUtils.removeTab(tab);
  });
  await onAdded;

  let ftuPref = SpecialPowers.getBoolPref(FTU_PREF);
  is(ftuPref, false, "First time use flag is false");

  await SpecialPowers.popPrefEnv();
});
