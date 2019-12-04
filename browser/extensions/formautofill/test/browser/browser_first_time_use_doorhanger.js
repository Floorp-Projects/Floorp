/* eslint-disable mozilla/no-arbitrary-setTimeout */
"use strict";

add_task(async function test_first_time_save() {
  let addresses = await getAddresses();
  is(addresses.length, 0, "No address in storage");
  await SpecialPowers.pushPrefEnv({
    set: [[FTU_PREF, true], [ENABLED_AUTOFILL_ADDRESSES_PREF, true]],
  });

  await BrowserTestUtils.withNewTab({ gBrowser, url: FORM_URL }, async function(
    browser
  ) {
    let promiseShown = BrowserTestUtils.waitForEvent(
      PopupNotifications.panel,
      "popupshown"
    );
    let tabPromise = BrowserTestUtils.waitForNewTab(
      gBrowser,
      "about:preferences#privacy"
    );
    await ContentTask.spawn(browser, null, async function() {
      let form = content.document.getElementById("form");
      form.querySelector("#organization").focus();
      form.querySelector("#organization").value = "Sesame Street";
      form.querySelector("#street-address").value = "123 Sesame Street";
      form.querySelector("#tel").value = "1-345-345-3456";

      // Wait 500ms before submission to make sure the input value applied
      await new Promise(resolve => setTimeout(resolve, 500));
      form.querySelector("input[type=submit]").click();
    });

    await promiseShown;
    let cb = getDoorhangerCheckbox();
    ok(cb.hidden, "Sync checkbox should be hidden");
    // Open the panel via main button
    await clickDoorhangerButton(MAIN_BUTTON);
    let tab = await tabPromise;
    ok(tab, "Privacy panel opened");
    BrowserTestUtils.removeTab(tab);
  });

  addresses = await getAddresses();
  is(addresses.length, 1, "Address saved");
  let ftuPref = SpecialPowers.getBoolPref(FTU_PREF);
  is(ftuPref, false, "First time use flag is false");
});

add_task(async function test_non_first_time_save() {
  let addresses = await getAddresses();
  let ftuPref = SpecialPowers.getBoolPref(FTU_PREF);
  is(ftuPref, false, "First time use flag is false");
  is(addresses.length, 1, "1 address in storage");

  await BrowserTestUtils.withNewTab({ gBrowser, url: FORM_URL }, async function(
    browser
  ) {
    await ContentTask.spawn(browser, null, async function() {
      let form = content.document.getElementById("form");
      form.querySelector("#organization").focus();
      form.querySelector("#organization").value = "Mozilla";
      form.querySelector("#street-address").value = "331 E. Evelyn Avenue";
      form.querySelector("#tel").value = "1-650-903-0800";

      // Wait 500ms before submission to make sure the input value applied
      await new Promise(resolve => setTimeout(resolve, 500));
      form.querySelector("input[type=submit]").click();
    });

    await sleep(1000);
    is(PopupNotifications.panel.state, "closed", "Doorhanger is hidden");
  });

  addresses = await getAddresses();
  is(addresses.length, 2, "Another address saved");
});

add_task(async function test_first_time_save_with_sync_account() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [FTU_PREF, true],
      [ENABLED_AUTOFILL_ADDRESSES_PREF, true],
      [SYNC_USERNAME_PREF, "foo@bar.com"],
    ],
  });

  await BrowserTestUtils.withNewTab({ gBrowser, url: FORM_URL }, async function(
    browser
  ) {
    let promiseShown = BrowserTestUtils.waitForEvent(
      PopupNotifications.panel,
      "popupshown"
    );
    let tabPromise = BrowserTestUtils.waitForNewTab(
      gBrowser,
      "about:preferences#privacy-address-autofill"
    );
    await ContentTask.spawn(browser, null, async function() {
      let form = content.document.getElementById("form");
      form.querySelector("#organization").focus();
      form.querySelector("#organization").value = "Foobar";
      form.querySelector("#email").value = "foo@bar.com";
      form.querySelector("#tel").value = "1-234-567-8900";

      // Wait 500ms before submission to make sure the input value applied
      await new Promise(resolve => setTimeout(resolve, 500));
      form.querySelector("input[type=submit]").click();
    });

    await promiseShown;
    let cb = getDoorhangerCheckbox();
    ok(!cb.hidden, "Sync checkbox should be visible");
    is(
      SpecialPowers.getBoolPref(SYNC_ADDRESSES_PREF),
      false,
      "addresses sync should be disabled at first"
    );

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

  let ftuPref = SpecialPowers.getBoolPref(FTU_PREF);
  is(ftuPref, false, "First time use flag is false");
});
