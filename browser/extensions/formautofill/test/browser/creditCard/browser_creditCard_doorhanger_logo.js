"use strict";

/*
  The next four tests look very similar because if we try to do multiple
  credit card operations in one test, there's a good chance the test will timeout
  and produce an invalid result.
  We mitigate this issue by having each test only deal with one credit card in storage
  and one credit card operation.
*/
add_task(async function test_submit_third_party_creditCard_logo() {
  const amexCard = {
    "cc-number": "374542158116607",
    "cc-type": "amex",
    "cc-name": "John Doe",
  };
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CREDITCARD_FORM_URL },
    async function (browser) {
      let onPopupShown = waitForPopupShown();
      await focusUpdateSubmitForm(browser, {
        focusSelector: "#cc-name",
        newValues: {
          "#cc-name": "User 1",
          "#cc-number": amexCard["cc-number"],
        },
      });

      await onPopupShown;
      let doorhanger = getNotification();
      let creditCardLogo = doorhanger.querySelector(".desc-message-box image");
      let creditCardLogoWithoutExtension = creditCardLogo.src.split(".", 1)[0];

      is(
        creditCardLogoWithoutExtension,
        "chrome://formautofill/content/third-party/cc-logo-amex",
        "CC logo should be amex"
      );

      await clickDoorhangerButton(SECONDARY_BUTTON);
    }
  );
  await removeAllRecords();
});

add_task(async function test_update_third_party_creditCard_logo() {
  const amexCard = {
    "cc-number": "374542158116607",
    "cc-name": "John Doe",
  };

  await setStorage(amexCard);
  let creditCards = await getCreditCards();
  is(creditCards.length, 1, "1 credit card in storage");

  let onChanged = waitForStorageChangedEvents("update");
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CREDITCARD_FORM_URL },
    async function (browser) {
      let onPopupShown = waitForPopupShown();
      await focusUpdateSubmitForm(browser, {
        focusSelector: "#cc-name",
        newValues: {
          "#cc-name": "Mark Smith",
          "#cc-number": amexCard["cc-number"],
          "#cc-exp-month": "4",
          "#cc-exp-year": new Date().getFullYear(),
        },
      });

      await onPopupShown;

      let doorhanger = getNotification();
      let creditCardLogo = doorhanger.querySelector(".desc-message-box image");
      let creditCardLogoWithoutExtension = creditCardLogo.src.split(".", 1)[0];
      is(
        creditCardLogoWithoutExtension,
        `chrome://formautofill/content/third-party/cc-logo-amex`,
        `CC Logo should be amex`
      );
      await clickDoorhangerButton(MAIN_BUTTON);
    }
  );
  await onChanged;
  await removeAllRecords();
});

add_task(async function test_submit_generic_creditCard_logo() {
  const genericCard = {
    "cc-number": "937899583135",
    "cc-name": "John Doe",
  };
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CREDITCARD_FORM_URL },
    async function (browser) {
      let onPopupShown = waitForPopupShown();
      await focusUpdateSubmitForm(browser, {
        focusSelector: "#cc-name",
        newValues: {
          "#cc-name": "User 1",
          "#cc-number": genericCard["cc-number"],
        },
      });

      await onPopupShown;
      let doorhanger = getNotification();
      let creditCardLogo = doorhanger.querySelector(".desc-message-box image");
      let creditCardLogoWithoutExtension = creditCardLogo.src.split(".", 1)[0];

      is(
        creditCardLogoWithoutExtension,
        "chrome://formautofill/content/icon-credit-card-generic",
        "CC logo should be generic"
      );

      await clickDoorhangerButton(SECONDARY_BUTTON);
    }
  );
  await removeAllRecords();
});

add_task(async function test_update_generic_creditCard_logo() {
  const genericCard = {
    "cc-number": "937899583135",
    "cc-name": "John Doe",
  };

  await setStorage(genericCard);
  let creditCards = await getCreditCards();
  is(creditCards.length, 1, "1 credit card in storage");

  let onChanged = waitForStorageChangedEvents("update");
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CREDITCARD_FORM_URL },
    async function (browser) {
      let onPopupShown = waitForPopupShown();
      await focusUpdateSubmitForm(browser, {
        focusSelector: "#cc-name",
        newValues: {
          "#cc-name": "Mark Smith",
          "#cc-number": genericCard["cc-number"],
          "#cc-exp-month": "4",
          "#cc-exp-year": new Date().getFullYear(),
        },
      });

      await onPopupShown;

      let doorhanger = getNotification();
      let creditCardLogo = doorhanger.querySelector(".desc-message-box image");
      let creditCardLogoWithoutExtension = creditCardLogo.src.split(".", 1)[0];
      is(
        creditCardLogoWithoutExtension,
        `chrome://formautofill/content/icon-credit-card-generic`,
        `CC Logo should be generic`
      );
      await clickDoorhangerButton(MAIN_BUTTON);
    }
  );
  await onChanged;
  await removeAllRecords();
});

add_task(async function test_save_panel_spaces_in_cc_number_logo() {
  const amexCard = {
    "cc-number": "37 4542 158116 607",
    "cc-type": "amex",
    "cc-name": "John Doe",
  };
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CREDITCARD_FORM_URL },
    async function (browser) {
      let onPopupShown = waitForPopupShown();
      await focusUpdateSubmitForm(browser, {
        focusSelector: "#cc-name",
        newValues: {
          "#cc-number": amexCard["cc-number"],
        },
      });

      await onPopupShown;
      let doorhanger = getNotification();
      let creditCardLogo = doorhanger.querySelector(".desc-message-box image");
      let creditCardLogoWithoutExtension = creditCardLogo.src.split(".", 1)[0];

      is(
        creditCardLogoWithoutExtension,
        "chrome://formautofill/content/third-party/cc-logo-amex",
        "CC logo should be amex"
      );

      await clickDoorhangerButton(SECONDARY_BUTTON);
    }
  );
});

add_task(async function test_update_panel_with_spaces_in_cc_number_logo() {
  const amexCard = {
    "cc-number": "374 54215 8116607",
    "cc-name": "John Doe",
  };

  await setStorage(amexCard);
  let creditCards = await getCreditCards();
  is(creditCards.length, 1, "1 credit card in storage");

  let onChanged = waitForStorageChangedEvents("update", "notifyUsed");
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CREDITCARD_FORM_URL },
    async function (browser) {
      let onPopupShown = waitForPopupShown();
      await focusUpdateSubmitForm(browser, {
        focusSelector: "#cc-name",
        newValues: {
          "#cc-name": "Mark Smith",
          "#cc-number": amexCard["cc-number"],
          "#cc-exp-month": "4",
          "#cc-exp-year": new Date().getFullYear(),
        },
      });

      await onPopupShown;

      let doorhanger = getNotification();
      let creditCardLogo = doorhanger.querySelector(".desc-message-box image");
      let creditCardLogoWithoutExtension = creditCardLogo.src.split(".", 1)[0];
      is(
        creditCardLogoWithoutExtension,
        `chrome://formautofill/content/third-party/cc-logo-amex`,
        `CC Logo should be amex`
      );
      // We are not testing whether the cc-number is saved correctly,
      // we only care that the logo in the update panel shows correctly.
      await clickDoorhangerButton(MAIN_BUTTON);
    }
  );
  await onChanged;
  await removeAllRecords();
});
