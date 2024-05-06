"use strict";

add_task(async function test_iframe_submit_untouched_creditCard_form() {
  if (!OSKeyStoreTestUtils.canTestOSKeyStoreLogin()) {
    todo(
      OSKeyStoreTestUtils.canTestOSKeyStoreLogin(),
      "Cannot test OS key store login on official builds."
    );
    return;
  }

  await setStorage(TEST_CREDIT_CARD_1);
  let creditCards = await getCreditCards();
  is(creditCards.length, 1, "1 credit card in storage");

  // This test triggers two form submission events so cc 'timesUsed' count is 2.
  // The first submission is triggered by standard form submission, and the
  // second is triggered by page hiding.
  const EXPECTED_ON_USED_COUNT = 2;
  let notifyUsedCounter = EXPECTED_ON_USED_COUNT;
  let onUsed = TestUtils.topicObserved(
    "formautofill-storage-changed",
    (subject, data) => {
      if (data == "notifyUsed") {
        notifyUsedCounter--;
      }
      return notifyUsedCounter == 0;
    }
  );
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CREDITCARD_FORM_IFRAME_URL },
    async function (browser) {
      let osKeyStoreLoginShown =
        OSKeyStoreTestUtils.waitForOSKeyStoreLogin(true);
      let iframeBC = browser.browsingContext.children[0];
      await openPopupOnSubframe(browser, iframeBC, "form #cc-name");

      EventUtils.synthesizeKey("VK_DOWN", {});
      EventUtils.synthesizeKey("VK_RETURN", {});
      await osKeyStoreLoginShown;
      await waitForAutofill(iframeBC, "#cc-name", "John Doe");

      await SpecialPowers.spawn(iframeBC, [], async function () {
        let form = content.document.getElementById("form");
        form.querySelector("input[type=submit]").click();
      });

      await sleep(1000);
      is(PopupNotifications.panel.state, "closed", "Doorhanger is hidden");
    }
  );
  await onUsed;

  creditCards = await getCreditCards();
  is(creditCards.length, 1, "Still 1 credit card");
  is(
    creditCards[0].timesUsed,
    EXPECTED_ON_USED_COUNT,
    "timesUsed field set to 2"
  );
  await removeAllRecords();
});

add_task(async function test_iframe_unload_save_card() {
  let onChanged = waitForStorageChangedEvents("add");
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CREDITCARD_FORM_IFRAME_URL },
    async function (browser) {
      let onPopupShown = waitForPopupShown();
      let iframeBC = browser.browsingContext.children[0];
      await focusUpdateSubmitForm(
        iframeBC,
        {
          focusSelector: "#cc-name",
          newValues: {
            "#cc-name": "User 1",
            "#cc-number": "4556194630960970",
            "#cc-exp-month": "10",
            "#cc-exp-year": "2024",
            "#cc-type": "visa",
          },
        },
        false
      );

      info("Removing iframe without submitting");
      await SpecialPowers.spawn(browser, [], async function () {
        let frame = content.document.querySelector("iframe");
        frame.remove();
      });

      await onPopupShown;
      await clickDoorhangerButton(MAIN_BUTTON);
    }
  );
  await onChanged;

  let creditCards = await getCreditCards();
  is(creditCards.length, 1, "1 credit card in storage");
  is(creditCards[0]["cc-name"], "User 1", "Verify the name field");
  is(creditCards[0]["cc-type"], "visa", "Verify the cc-type field");
  await removeAllRecords();
});
