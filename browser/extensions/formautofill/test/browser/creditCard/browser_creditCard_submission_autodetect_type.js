/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_autodetect_credit_not_set() {
  const testCard = {
    "cc-name": "John Doe",
    "cc-number": "4012888888881881",
    "cc-exp-month": "06",
    "cc-exp-year": "2044",
  };
  const expectedData = {
    ...testCard,
    ...{ "cc-type": "visa" },
  };
  let onChanged = waitForStorageChangedEvents("add");

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CREDITCARD_FORM_URL },
    async function (browser) {
      let promiseShown = waitForPopupShown();
      await focusUpdateSubmitForm(browser, {
        focusSelector: "#cc-name",
        newValues: {
          "#cc-name": testCard["cc-name"],
          "#cc-number": testCard["cc-number"],
          "#cc-exp-month": testCard["cc-exp-month"],
          "#cc-exp-year": testCard["cc-exp-year"],
          "#cc-type": testCard["cc-type"],
        },
      });

      await promiseShown;
      await clickDoorhangerButton(MAIN_BUTTON);
    }
  );

  await onChanged;

  let creditCards = await getCreditCards();
  let savedCreditCard = creditCards[0];
  let decryptedNumber = await OSKeyStore.decrypt(
    savedCreditCard["cc-number-encrypted"]
  );
  savedCreditCard["cc-number"] = decryptedNumber;
  for (let key in testCard) {
    let expected = expectedData[key];
    let actual = savedCreditCard[key];
    Assert.equal(expected, actual, `${key} should match`);
  }
  await removeAllRecords();
});

add_task(async function test_autodetect_credit_overwrite() {
  const testCard = {
    "cc-name": "John Doe",
    "cc-number": "4012888888881881",
    "cc-exp-month": "06",
    "cc-exp-year": "2044",
    "cc-type": "master", // Wrong credit card type
  };
  const expectedData = {
    ...testCard,
    ...{ "cc-type": "visa" },
  };
  let onChanged = waitForStorageChangedEvents("add");

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CREDITCARD_FORM_URL },
    async function (browser) {
      let promiseShown = waitForPopupShown();
      await focusUpdateSubmitForm(browser, {
        focusSelector: "#cc-name",
        newValues: {
          "#cc-name": testCard["cc-name"],
          "#cc-number": testCard["cc-number"],
          "#cc-exp-month": testCard["cc-exp-month"],
          "#cc-exp-year": testCard["cc-exp-year"],
          "#cc-type": testCard["cc-type"],
        },
      });

      await promiseShown;
      await clickDoorhangerButton(MAIN_BUTTON);
    }
  );

  await onChanged;

  let creditCards = await getCreditCards();
  let savedCreditCard = creditCards[0];
  let decryptedNumber = await OSKeyStore.decrypt(
    savedCreditCard["cc-number-encrypted"]
  );
  savedCreditCard["cc-number"] = decryptedNumber;
  for (let key in testCard) {
    let expected = expectedData[key];
    let actual = savedCreditCard[key];
    Assert.equal(expected, actual, `${key} should match`);
  }

  await removeAllRecords();
});
