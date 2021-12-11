"use strict";

add_task(async function test_fill_creditCard_but_cancel_login() {
  if (!OSKeyStoreTestUtils.canTestOSKeyStoreLogin()) {
    todo(
      OSKeyStoreTestUtils.canTestOSKeyStoreLogin(),
      "Cannot test OS key store login on official builds."
    );
    return;
  }

  await saveCreditCard(TEST_CREDIT_CARD_2);

  let osKeyStoreLoginShown = OSKeyStoreTestUtils.waitForOSKeyStoreLogin(false); // cancel
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CREDITCARD_FORM_URL },
    async function(browser) {
      await openPopupOn(browser, "#cc-name");
      const ccItem = getDisplayedPopupItems(browser)[0];
      let popupClosePromise = BrowserTestUtils.waitForPopupEvent(
        browser.autoCompletePopup,
        "hidden"
      );
      await EventUtils.synthesizeMouseAtCenter(ccItem, {});
      await Promise.all([osKeyStoreLoginShown, popupClosePromise]);

      await SpecialPowers.spawn(browser, [], async function() {
        is(content.document.querySelector("#cc-name").value, "", "Check name");
        is(
          content.document.querySelector("#cc-number").value,
          "",
          "Check number"
        );
      });
    }
  );
});
