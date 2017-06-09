/* import-globals-from ../../../../../testing/mochitest/tests/SimpleTest/SimpleTest.js */
/* import-globals-from ../../../../../toolkit/components/satchel/test/satchel_common.js */
/* eslint-disable no-unused-vars */

"use strict";

let formFillChromeScript;

function setInput(selector, value) {
  let input = document.querySelector("input" + selector);
  input.value = value;
  input.focus();

  // "identifyAutofillFields" is invoked asynchronously in "focusin" event. We
  // should make sure fields are ready for popup before doing tests.
  //
  // TODO: "setTimeout" is used here temporarily because there's no event to
  //       notify us of the state of "identifyAutofillFields" for now. We should
  //       figure out a better way after the heuristics land.
  SimpleTest.requestFlakyTimeout("Guarantee asynchronous identifyAutofillFields is invoked");
  return new Promise(resolve => setTimeout(() => {
    resolve(input);
  }, 500));
}

function checkMenuEntries(expectedValues) {
  let actualValues = getMenuEntries();

  is(actualValues.length, expectedValues.length, " Checking length of expected menu");
  for (let i = 0; i < expectedValues.length; i++) {
    is(actualValues[i], expectedValues[i], " Checking menu entry #" + i);
  }
}

function addAddress(address) {
  return new Promise(resolve => {
    formFillChromeScript.sendAsyncMessage("FormAutofillTest:AddAddress", {address});
    formFillChromeScript.addMessageListener("FormAutofillTest:AddressAdded", function onAdded(data) {
      formFillChromeScript.removeMessageListener("FormAutofillTest:AddressAdded", onAdded);

      resolve();
    });
  });
}

function removeAddress(guid) {
  return new Promise(resolve => {
    formFillChromeScript.sendAsyncMessage("FormAutofillTest:RemoveAddress", {guid});
    formFillChromeScript.addMessageListener("FormAutofillTest:AddressRemoved", function onDeleted(data) {
      formFillChromeScript.removeMessageListener("FormAutofillTest:AddressRemoved", onDeleted);

      resolve();
    });
  });
}

function updateAddress(guid, address) {
  return new Promise(resolve => {
    formFillChromeScript.sendAsyncMessage("FormAutofillTest:UpdateAddress", {address, guid});
    formFillChromeScript.addMessageListener("FormAutofillTest:AddressUpdated", function onUpdated(data) {
      formFillChromeScript.removeMessageListener("FormAutofillTest:AddressUpdated", onUpdated);

      resolve();
    });
  });
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
  });
}

formAutoFillCommonSetup();
