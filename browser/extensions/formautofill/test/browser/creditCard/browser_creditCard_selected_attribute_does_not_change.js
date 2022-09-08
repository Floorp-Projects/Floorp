/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_selected_attribute_does_not_change_after_fill() {
  await setStorage(TEST_CREDIT_CARD_1);
  let osKeyStoreLoginShown = OSKeyStoreTestUtils.waitForOSKeyStoreLogin(true);
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    CREDITCARD_FORM_WITH_SELECTED_OPTION_URL
  );
  let browser = gBrowser.selectedBrowser;
  let expectedElement = await SpecialPowers.spawn(browser, [], async () => {
    let option = content.document.querySelector(`option[selected="true"]`)
      .value;
    return option;
  });

  await openPopupOn(browser, "form #cc-name");
  await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);
  await BrowserTestUtils.synthesizeKey("VK_RETURN", {}, browser);
  await osKeyStoreLoginShown;
  await waitForAutofill(browser, "#cc-number", TEST_CREDIT_CARD_1["cc-number"]);

  await SpecialPowers.spawn(browser, [expectedElement], async expected => {
    let selectedElement = content.document.querySelector(
      `option[selected="true"]`
    ).value;
    Assert.equal(
      selectedElement,
      expected,
      "Selected attribute should not change after fill"
    );
  });

  await BrowserTestUtils.removeTab(tab);
});
