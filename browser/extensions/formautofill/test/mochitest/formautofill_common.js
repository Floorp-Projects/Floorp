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

function addProfile(profile) {
  return new Promise(resolve => {
    formFillChromeScript.sendAsyncMessage("FormAutofillTest:AddProfile", {profile});
    formFillChromeScript.addMessageListener("FormAutofillTest:ProfileAdded", function onAdded(data) {
      formFillChromeScript.removeMessageListener("FormAutofillTest:ProfileAdded", onAdded);

      resolve();
    });
  });
}

function removeProfile(guid) {
  return new Promise(resolve => {
    formFillChromeScript.sendAsyncMessage("FormAutofillTest:RemoveProfile", {guid});
    formFillChromeScript.addMessageListener("FormAutofillTest:ProfileRemoved", function onDeleted(data) {
      formFillChromeScript.removeMessageListener("FormAutofillTest:ProfileRemoved", onDeleted);

      resolve();
    });
  });
}

function updateProfile(guid, profile) {
  return new Promise(resolve => {
    formFillChromeScript.sendAsyncMessage("FormAutofillTest:UpdateProfile", {profile, guid});
    formFillChromeScript.addMessageListener("FormAutofillTest:ProfileUpdated", function onUpdated(data) {
      formFillChromeScript.removeMessageListener("FormAutofillTest:ProfileUpdated", onUpdated);

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
