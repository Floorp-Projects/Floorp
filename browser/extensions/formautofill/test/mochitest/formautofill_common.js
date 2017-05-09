/* import-globals-from ../../../../../testing/mochitest/tests/SimpleTest/SimpleTest.js */
/* import-globals-from ../../../../../toolkit/components/satchel/test/satchel_common.js */
/* eslint-disable no-unused-vars */

"use strict";

let formFillChromeScript;

function setInput(selector, value) {
  let input = document.querySelector("input" + selector);
  input.value = value;
  input.focus();
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
  SpecialPowers.setBoolPref("dom.forms.autocomplete.experimental", true);
  formFillChromeScript.addMessageListener("onpopupshown", ({results}) => {
    gLastAutoCompleteResults = results;
    if (gPopupShownListener) {
      gPopupShownListener({results});
    }
  });

  SimpleTest.registerCleanupFunction(() => {
    SpecialPowers.clearUserPref("dom.forms.autocomplete.experimental");
    formFillChromeScript.sendAsyncMessage("cleanup");
    formFillChromeScript.destroy();
  });
}

formAutoFillCommonSetup();
