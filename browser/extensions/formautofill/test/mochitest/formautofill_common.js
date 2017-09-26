/* import-globals-from ../../../../../testing/mochitest/tests/SimpleTest/SimpleTest.js */
/* import-globals-from ../../../../../toolkit/components/satchel/test/satchel_common.js */
/* eslint-disable no-unused-vars */

"use strict";

let formFillChromeScript;
let expectingPopup = null;

async function sleep(ms = 500, reason = "Intentionally wait for UI ready") {
  SimpleTest.requestFlakyTimeout(reason);
  await new Promise(resolve => setTimeout(resolve, ms));
}

async function setInput(selector, value) {
  let input = document.querySelector("input" + selector);
  input.value = value;
  input.focus();

  // "identifyAutofillFields" is invoked asynchronously in "focusin" event. We
  // should make sure fields are ready for popup before doing tests.
  //
  // TODO: "sleep" is used here temporarily because there's no event to
  //       notify us of the state of "identifyAutofillFields" for now. We should
  //       figure out a better way after the heuristics land.
  await sleep(500, "Guarantee asynchronous identifyAutofillFields is invoked");
  return input;
}

function clickOnElement(selector) {
  let element = document.querySelector(selector);

  if (!element) {
    throw new Error("Can not find the element");
  }

  SimpleTest.executeSoon(() => element.click());
}

async function onStorageChanged(type) {
  return new Promise(resolve => {
    formFillChromeScript.addMessageListener("formautofill-storage-changed", function onChanged(data) {
      formFillChromeScript.removeMessageListener("formautofill-storage-changed", onChanged);
      is(data.data, type, `Receive ${type} storage changed event`);
      resolve();
    });
  });
}

function checkMenuEntries(expectedValues, isFormAutofillResult = true) {
  let actualValues = getMenuEntries();
  // Expect one more item would appear at the bottom as the footer if the result is from form autofill.
  let expectedLength = isFormAutofillResult ? expectedValues.length + 1 : expectedValues.length;

  is(actualValues.length, expectedLength, " Checking length of expected menu");
  for (let i = 0; i < expectedValues.length; i++) {
    is(actualValues[i], expectedValues[i], " Checking menu entry #" + i);
  }
}

function invokeAsyncChromeTask(message, response, payload = {}) {
  return new Promise(resolve => {
    formFillChromeScript.sendAsyncMessage(message, payload);
    formFillChromeScript.addMessageListener(response, function onReceived(data) {
      formFillChromeScript.removeMessageListener(response, onReceived);

      resolve(data);
    });
  });
}

async function addAddress(address) {
  await invokeAsyncChromeTask("FormAutofillTest:AddAddress", "FormAutofillTest:AddressAdded", {address});
  await sleep();
}

async function removeAddress(guid) {
  return invokeAsyncChromeTask("FormAutofillTest:RemoveAddress", "FormAutofillTest:AddressRemoved", {guid});
}

async function updateAddress(guid, address) {
  return invokeAsyncChromeTask("FormAutofillTest:UpdateAddress", "FormAutofillTest:AddressUpdated", {address, guid});
}

async function checkAddresses(expectedAddresses) {
  return invokeAsyncChromeTask("FormAutofillTest:CheckAddresses", "FormAutofillTest:areAddressesMatching", {expectedAddresses});
}

async function cleanUpAddresses() {
  return invokeAsyncChromeTask("FormAutofillTest:CleanUpAddresses", "FormAutofillTest:AddressesCleanedUp");
}

async function addCreditCard(creditcard) {
  await invokeAsyncChromeTask("FormAutofillTest:AddCreditCard", "FormAutofillTest:CreditCardAdded", {creditcard});
  await sleep();
}

async function removeCreditCard(guid) {
  return invokeAsyncChromeTask("FormAutofillTest:RemoveCreditCard", "FormAutofillTest:CreditCardRemoved", {guid});
}

async function checkCreditCards(expectedCreditCards) {
  return invokeAsyncChromeTask("FormAutofillTest:CheckCreditCards", "FormAutofillTest:areCreditCardsMatching", {expectedCreditCards});
}

async function cleanUpCreditCards() {
  return invokeAsyncChromeTask("FormAutofillTest:CleanUpCreditCards", "FormAutofillTest:CreditCardsCleanedUp");
}

async function cleanUpStorage() {
  await cleanUpAddresses();
  await cleanUpCreditCards();
}

// Utils for registerPopupShownListener(in satchel_common.js) that handles dropdown popup
// Please call "initPopupListener()" in your test and "await expectPopup()"
// if you want to wait for dropdown menu displayed.
function expectPopup() {
  info("expecting a popup");
  return new Promise(resolve => {
    expectingPopup = resolve;
  });
}

function popupShownListener() {
  info("popup shown for test ");
  if (expectingPopup) {
    expectingPopup();
    expectingPopup = null;
  }
}

function initPopupListener() {
  registerPopupShownListener(popupShownListener);
}

function formAutoFillCommonSetup() {
  let chromeURL = SimpleTest.getTestFileURL("formautofill_parent_utils.js");
  formFillChromeScript = SpecialPowers.loadChromeScript(chromeURL);
  formFillChromeScript.addMessageListener("onpopupshown", ({results}) => {
    gLastAutoCompleteResults = results;
    if (gPopupShownListener) {
      gPopupShownListener({results});
    }
  });

  SimpleTest.registerCleanupFunction(() => {
    formFillChromeScript.sendAsyncMessage("cleanup");
    formFillChromeScript.destroy();
    expectingPopup = null;
  });
}

formAutoFillCommonSetup();
