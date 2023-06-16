/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// We want to ensure that non-normalized credit card data is normalized
// correctly as part of the save credit card flow
add_task(async function test_new_submitted_card_is_normalized() {
  const testCard = {
    "cc-name": "Test User",
    "cc-number": "5038146897157463",
    "cc-exp-month": "4",
    "cc-exp-year": "25",
  };
  const expectedData = {
    "cc-name": "Test User",
    "cc-number": "5038146897157463",
    "cc-exp-month": "4",
    // cc-exp-year should be normalized to 2025
    "cc-exp-year": "2025",
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

add_task(async function test_updated_card_is_normalized() {
  const testCard = {
    "cc-name": "Test User",
    "cc-number": "5038146897157463",
    "cc-exp-month": "11",
    "cc-exp-year": "20",
  };
  await saveCreditCard(testCard);
  const expectedData = {
    "cc-name": "Test User",
    "cc-number": "5038146897157463",
    "cc-exp-month": "10",
    // cc-exp-year should be normalized to 2027
    "cc-exp-year": "2027",
  };
  let onChanged = waitForStorageChangedEvents("update");

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CREDITCARD_FORM_URL },
    async function (browser) {
      let promiseShown = waitForPopupShown();
      await focusUpdateSubmitForm(browser, {
        focusSelector: "#cc-name",
        newValues: {
          "#cc-name": testCard["cc-name"],
          "#cc-number": testCard["cc-number"],
          "#cc-exp-month": "10",
          "#cc-exp-year": "27",
        },
      });

      await promiseShown;
      await clickDoorhangerButton(MAIN_BUTTON);
    }
  );

  await onChanged;

  let creditCards = await getCreditCards();
  let savedCreditCard = creditCards[0];
  savedCreditCard["cc-number"] = await OSKeyStore.decrypt(
    savedCreditCard["cc-number-encrypted"]
  );

  for (let key in testCard) {
    let expected = expectedData[key];
    let actual = savedCreditCard[key];
    Assert.equal(expected, actual, `${key} should match`);
  }
  await removeAllRecords();
});
