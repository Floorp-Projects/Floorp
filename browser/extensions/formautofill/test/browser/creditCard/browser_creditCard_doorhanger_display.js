"use strict";
/*
  We use arbitrary timeouts here to ensure that the input event queue is cleared
  before we assert any information about the UI, otherwise there's a reasonable chance
  that the UI won't be ready and we will get an invalid test result.
*/
add_task(async function test_doorhanger_not_shown_when_autofill_untouched() {
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

  let onUsed = waitForStorageChangedEvents("notifyUsed");
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CREDITCARD_FORM_URL },
    async function(browser) {
      let osKeyStoreLoginShown = OSKeyStoreTestUtils.waitForOSKeyStoreLogin(
        true
      );
      await openPopupOn(browser, "form #cc-name");
      await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);
      await BrowserTestUtils.synthesizeKey("VK_RETURN", {}, browser);
      await osKeyStoreLoginShown;
      await waitForAutofill(browser, "#cc-name", "John Doe");

      await SpecialPowers.spawn(browser, [], async function() {
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
  is(creditCards[0].timesUsed, 1, "timesUsed field set to 1");
  await removeAllRecords();
});

add_task(async function test_doorhanger_not_shown_when_fill_duplicate() {
  await setStorage(TEST_CREDIT_CARD_1);
  let creditCards = await getCreditCards();
  is(creditCards.length, 1, "1 credit card in storage");

  let onUsed = waitForStorageChangedEvents("notifyUsed");
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CREDITCARD_FORM_URL },
    async function(browser) {
      await focusUpdateSubmitForm(browser, {
        focusSelector: "#cc-name",
        newValues: {
          "#cc-name": "John Doe",
          "#cc-number": "4111111111111111",
          "#cc-exp-month": "4",
          "#cc-exp-year": new Date().getFullYear(),
          "#cc-type": "visa",
        },
      });

      await sleep(1000);
      is(PopupNotifications.panel.state, "closed", "Doorhanger is hidden");
    }
  );
  await onUsed;

  creditCards = await getCreditCards();
  is(creditCards.length, 1, "Still 1 credit card in storage");
  is(
    creditCards[0]["cc-name"],
    TEST_CREDIT_CARD_1["cc-name"],
    "Verify the name field"
  );
  is(creditCards[0].timesUsed, 1, "timesUsed field set to 1");
  await removeAllRecords();
});

add_task(
  async function test_doorhanger_not_shown_when_autofill_then_fill_duplicate() {
    if (!OSKeyStoreTestUtils.canTestOSKeyStoreLogin()) {
      todo(
        OSKeyStoreTestUtils.canTestOSKeyStoreLogin(),
        "Cannot test OS key store login on official builds."
      );
      return;
    }

    await setStorage(
      { "cc-number": "6387060366272981" },
      { "cc-number": "5038146897157463" }
    );
    let creditCards = await getCreditCards();
    is(creditCards.length, 2, "2 credit card in storage");
    let onUsed = waitForStorageChangedEvents("notifyUsed");
    await BrowserTestUtils.withNewTab(
      { gBrowser, url: CREDITCARD_FORM_URL },
      async function(browser) {
        let osKeyStoreLoginShown = OSKeyStoreTestUtils.waitForOSKeyStoreLogin(
          true
        );
        await openPopupOn(browser, "form #cc-number");
        await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);
        await BrowserTestUtils.synthesizeKey("VK_RETURN", {}, browser);
        await waitForAutofill(browser, "#cc-number", "6387060366272981");

        await focusUpdateSubmitForm(browser, {
          focusSelector: "#cc-name",
          newValues: {
            // Change number to the second credit card number
            "#cc-number": "5038146897157463",
          },
        });

        await sleep(1000);
        is(PopupNotifications.panel.state, "closed", "Doorhanger is hidden");
        await osKeyStoreLoginShown;
      }
    );
    await onUsed;

    creditCards = await getCreditCards();
    is(creditCards.length, 2, "Still 2 credit card");
    await removeAllRecords();
  }
);

add_task(async function test_update_doorhanger_shown_when_fill_mergeable() {
  await setStorage(TEST_CREDIT_CARD_3);
  let creditCards = await getCreditCards();
  is(creditCards.length, 1, "1 credit card in storage");

  let onChanged = waitForStorageChangedEvents("update", "notifyUsed");
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CREDITCARD_FORM_URL },
    async function(browser) {
      let onPopupShown = waitForPopupShown();
      await focusUpdateSubmitForm(browser, {
        focusSelector: "#cc-name",
        newValues: {
          "#cc-name": "User 3",
          "#cc-number": "5103059495477870",
          "#cc-exp-month": "1",
          "#cc-exp-year": "2000",
        },
      });

      await onPopupShown;
      await clickDoorhangerButton(MAIN_BUTTON);
    }
  );
  await onChanged;

  creditCards = await getCreditCards();
  is(creditCards.length, 1, "Still 1 credit card in storage");
  is(creditCards[0]["cc-name"], "User 3", "Verify the name field");
  await removeAllRecords();
});
