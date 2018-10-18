"use strict";

add_task(async function test_fill_creditCard_with_mp_enabled_but_canceled() {
  await saveCreditCard(TEST_CREDIT_CARD_2);

  LoginTestUtils.masterPassword.enable();
  registerCleanupFunction(() => {
    LoginTestUtils.masterPassword.disable();
  });

  let masterPasswordDialogShown = waitForMasterPasswordDialog(false); // cancel
  await BrowserTestUtils.withNewTab({gBrowser, url: CREDITCARD_FORM_URL},
    async function(browser) {
      await openPopupOn(browser, "#cc-name");
      const ccItem = getDisplayedPopupItems(browser)[0];
      await EventUtils.synthesizeMouseAtCenter(ccItem, {});
      await Promise.all([masterPasswordDialogShown, expectPopupClose(browser)]);

      await ContentTask.spawn(browser, {}, async function() {
        is(content.document.querySelector("#cc-name").value, "", "Check name");
        is(content.document.querySelector("#cc-number").value, "", "Check number");
      });
    }
  );
});
