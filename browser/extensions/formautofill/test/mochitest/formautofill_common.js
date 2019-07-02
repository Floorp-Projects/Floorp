/* import-globals-from ../../../../../testing/mochitest/tests/SimpleTest/SimpleTest.js */
/* import-globals-from ../../../../../testing/mochitest/tests/SimpleTest/EventUtils.js */
/* import-globals-from ../../../../../toolkit/components/satchel/test/satchel_common.js */
/* eslint-disable no-unused-vars */

"use strict";

let formFillChromeScript;
let defaultTextColor;
let expectingPopup = null;

const {FormAutofillUtils} = SpecialPowers.Cu.import("resource://formautofill/FormAutofillUtils.jsm");

async function sleep(ms = 500, reason = "Intentionally wait for UI ready") {
  SimpleTest.requestFlakyTimeout(reason);
  await new Promise(resolve => setTimeout(resolve, ms));
}

async function focusAndWaitForFieldsIdentified(input, mustBeIdentified = false) {
  info("expecting the target input being focused and indentified");
  if (typeof input === "string") {
    input = document.querySelector(input);
  }
  const rootElement = input.form || input.ownerDocument.documentElement;
  const previouslyFocused = input != document.activeElement;

  input.focus();

  if (mustBeIdentified) {
    rootElement.removeAttribute("test-formautofill-identified");
  }
  if (rootElement.hasAttribute("test-formautofill-identified")) {
    return;
  }
  if (!previouslyFocused) {
    await new Promise(resolve => {
      formFillChromeScript.addMessageListener("FormAutofillTest:FieldsIdentified", function onIdentified() {
        formFillChromeScript.removeMessageListener("FormAutofillTest:FieldsIdentified", onIdentified);
        resolve();
      });
    });
  }
  // In order to ensure that "markAsAutofillField" is fully executed, a short period
  // of timeout is still required.
  await sleep(300, "Guarantee asynchronous identifyAutofillFields is invoked");
  rootElement.setAttribute("test-formautofill-identified", "true");
}

async function setInput(selector, value, userInput = false) {
  const input = document.querySelector("input" + selector);
  if (userInput) {
    SpecialPowers.wrap(input).setUserInput(value);
  } else {
    input.value = value;
  }
  await focusAndWaitForFieldsIdentified(input);

  return input;
}

function clickOnElement(selector) {
  let element = document.querySelector(selector);

  if (!element) {
    throw new Error("Can not find the element");
  }

  SimpleTest.executeSoon(() => element.click());
}

// The equivalent helper function to getAdaptedProfiles in FormAutofillHandler.jsm that
// transforms the given profile to expected filled profile.
function _getAdaptedProfile(profile) {
  const adaptedProfile = Object.assign({}, profile);

  if (profile["street-address"]) {
    adaptedProfile["street-address"] = FormAutofillUtils.toOneLineAddress(profile["street-address"]);
  }

  return adaptedProfile;
}

// We could not get ManuallyManagedState of element now, so directly check if
// filter and text color style are applied.
async function checkFieldHighlighted(elem, expectedValue) {
  let isHighlightApplied;
  await SimpleTest.promiseWaitForCondition(function checkHighlight() {
    const computedStyle = window.getComputedStyle(elem);
    isHighlightApplied = computedStyle.getPropertyValue("filter") !== "none";
    return isHighlightApplied === expectedValue;
  }, `Checking #${elem.id} highlight style`);

  is(isHighlightApplied, expectedValue, `Checking #${elem.id} highlight style`);
}

async function checkFieldPreview(elem, expectedValue) {
  is(SpecialPowers.wrap(elem).previewValue, expectedValue, `Checking #${elem.id} previewValue`);
  let isTextColorApplied;
  await SimpleTest.promiseWaitForCondition(function checkPreview() {
    const computedStyle = window.getComputedStyle(elem);
    isTextColorApplied = computedStyle.getPropertyValue("color") !== defaultTextColor;
    return isTextColorApplied === !!expectedValue;
  }, `Checking #${elem.id} preview style`);

  is(isTextColorApplied, !!expectedValue, `Checking #${elem.id} preview style`);
}

function checkFieldValue(elem, expectedValue) {
  if (typeof elem === "string") {
    elem = document.querySelector(elem);
  }
  is(elem.value, String(expectedValue), "Checking " + elem.id + " field");
}

function triggerAutofillAndCheckProfile(profile) {
  const adaptedProfile = _getAdaptedProfile(profile);
  const promises = [];

  for (const [fieldName, value] of Object.entries(adaptedProfile)) {
    info(`triggerAutofillAndCheckProfile: ${fieldName}`);
    const element = document.getElementById(fieldName);
    const expectingEvent = document.activeElement == element ? "input" : "change";
    const checkFieldAutofilled = Promise.all([
      new Promise(resolve => element.addEventListener("input", (event) => {
        if (element.tagName == "INPUT" && element.type == "text") {
          ok(event instanceof InputEvent,
             `"input" event should be dispatched with InputEvent interface on ${element.tagName}`);
          is(event.inputType, "insertReplacementText",
             "inputType value should be \"insertReplacementText\"");
          is(event.data, String(value),
             `data value should be "${value}"`);
          is(event.dataTransfer, null,
             "dataTransfer should be null");
        } else {
          ok(event instanceof Event && !(event instanceof UIEvent),
             `"input" event should be dispatched with Event interface on ${element.tagName}`);
        }
        is(event.cancelable, false,
           `"input" event should be never cancelable on ${element.tagName}`);
        is(event.bubbles, true,
           `"input" event should always bubble on ${element.tagName}`);
        resolve();
      }, {once: true})),
      new Promise(resolve => element.addEventListener(expectingEvent, resolve, {once: true})),
    ]).then(() => checkFieldValue(element, value));

    promises.push(checkFieldAutofilled);
  }
  // Press Enter key and trigger form autofill.
  synthesizeKey("KEY_Enter");

  return Promise.all(promises);
}

async function onStorageChanged(type) {
  info(`expecting the storage changed: ${type}`);
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
  info(`expecting the chrome task finished: ${message}`);
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

async function canTestOSKeyStoreLogin() {
  let {canTest} = await invokeAsyncChromeTask("FormAutofillTest:CanTestOSKeyStoreLogin", "FormAutofillTest:CanTestOSKeyStoreLoginResult");
  return canTest;
}

async function waitForOSKeyStoreLogin(login = false) {
  await invokeAsyncChromeTask("FormAutofillTest:OSKeyStoreLogin", "FormAutofillTest:OSKeyStoreLoggedIn", {login});
}

function patchRecordCCNumber(record) {
  const number = record["cc-number"];
  const ccNumberFmt = {
    affix: "****",
    label: number.substr(-4),
  };

  return Object.assign({}, record, {ccNumberFmt});
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

function notExpectPopup(ms = 500) {
  info("not expecting a popup");
  return new Promise((resolve, reject) => {
    expectingPopup = reject.bind(this, "Unexpected Popup");
    // TODO: We don't have an event to notify no popup showing, so wait for 500
    // ms (in default) to predict any unexpected popup showing.
    setTimeout(resolve, ms);
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

async function triggerPopupAndHoverItem(fieldSelector, selectIndex) {
  await focusAndWaitForFieldsIdentified(fieldSelector);
  synthesizeKey("KEY_ArrowDown");
  await expectPopup();
  for (let i = 0; i <= selectIndex; i++) {
    synthesizeKey("KEY_ArrowDown");
  }
  await notifySelectedIndex(selectIndex);
}

function formAutoFillCommonSetup() {
  // Remove the /creditCard path segement when referenced from the 'creditCard' subdirectory.
  let chromeURL = SimpleTest.getTestFileURL("formautofill_parent_utils.js").replace(/\/creditCard/, "");
  formFillChromeScript = SpecialPowers.loadChromeScript(chromeURL);
  formFillChromeScript.addMessageListener("onpopupshown", ({results}) => {
    gLastAutoCompleteResults = results;
    if (gPopupShownListener) {
      gPopupShownListener({results});
    }
  });

  add_task(async function setup() {
    formFillChromeScript.sendAsyncMessage("setup");
    info(`expecting the storage setup`);
    await formFillChromeScript.promiseOneMessage("setup-finished");
  });

  SimpleTest.registerCleanupFunction(async () => {
    formFillChromeScript.sendAsyncMessage("cleanup");
    info(`expecting the storage cleanup`);
    await formFillChromeScript.promiseOneMessage("cleanup-finished");

    formFillChromeScript.destroy();
    expectingPopup = null;
  });

  document.addEventListener("DOMContentLoaded", function() {
    defaultTextColor = window.getComputedStyle(document.querySelector("input"))
      .getPropertyValue("color");
  }, {once: true});
}

formAutoFillCommonSetup();
