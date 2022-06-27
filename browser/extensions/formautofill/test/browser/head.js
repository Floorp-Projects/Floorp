"use strict";

const { OSKeyStore } = ChromeUtils.import(
  "resource://gre/modules/OSKeyStore.jsm"
);
const { OSKeyStoreTestUtils } = ChromeUtils.import(
  "resource://testing-common/OSKeyStoreTestUtils.jsm"
);

const { FormAutofillParent } = ChromeUtils.import(
  "resource://autofill/FormAutofillParent.jsm"
);

const MANAGE_ADDRESSES_DIALOG_URL =
  "chrome://formautofill/content/manageAddresses.xhtml";
const MANAGE_CREDIT_CARDS_DIALOG_URL =
  "chrome://formautofill/content/manageCreditCards.xhtml";
const EDIT_ADDRESS_DIALOG_URL =
  "chrome://formautofill/content/editAddress.xhtml";
const EDIT_CREDIT_CARD_DIALOG_URL =
  "chrome://formautofill/content/editCreditCard.xhtml";
const PRIVACY_PREF_URL = "about:preferences#privacy";

const HTTP_TEST_PATH = "/browser/browser/extensions/formautofill/test/browser/";
const BASE_URL = "http://mochi.test:8888" + HTTP_TEST_PATH;
const FORM_URL = BASE_URL + "autocomplete_basic.html";
const CREDITCARD_FORM_URL =
  "https://example.org" +
  HTTP_TEST_PATH +
  "creditCard/autocomplete_creditcard_basic.html";
const CREDITCARD_FORM_IFRAME_URL =
  "https://example.org" +
  HTTP_TEST_PATH +
  "creditCard/autocomplete_creditcard_iframe.html";
const CREDITCARD_FORM_COMBINED_EXPIRY_URL =
  "https://example.org" +
  HTTP_TEST_PATH +
  "creditCard/autocomplete_creditcard_cc_exp_field.html";

const FTU_PREF = "extensions.formautofill.firstTimeUse";
const CREDITCARDS_USED_STATUS_PREF = "extensions.formautofill.creditCards.used";
const ENABLED_AUTOFILL_ADDRESSES_PREF =
  "extensions.formautofill.addresses.enabled";
const ENABLED_AUTOFILL_ADDRESSES_CAPTURE_PREF =
  "extensions.formautofill.addresses.capture.enabled";
const AUTOFILL_ADDRESSES_AVAILABLE_PREF =
  "extensions.formautofill.addresses.supported";
const ENABLED_AUTOFILL_ADDRESSES_SUPPORTED_COUNTRIES_PREF =
  "extensions.formautofill.addresses.supportedCountries";
const AUTOFILL_CREDITCARDS_AVAILABLE_PREF =
  "extensions.formautofill.creditCards.supported";
const ENABLED_AUTOFILL_CREDITCARDS_PREF =
  "extensions.formautofill.creditCards.enabled";
const SUPPORTED_COUNTRIES_PREF = "extensions.formautofill.supportedCountries";
const SYNC_USERNAME_PREF = "services.sync.username";
const SYNC_ADDRESSES_PREF = "services.sync.engine.addresses";
const SYNC_CREDITCARDS_PREF = "services.sync.engine.creditcards";
const SYNC_CREDITCARDS_AVAILABLE_PREF =
  "services.sync.engine.creditcards.available";

const TEST_ADDRESS_1 = {
  "given-name": "John",
  "additional-name": "R.",
  "family-name": "Smith",
  organization: "World Wide Web Consortium",
  "street-address": "32 Vassar Street\nMIT Room 32-G524",
  "address-level2": "Cambridge",
  "address-level1": "MA",
  "postal-code": "02139",
  country: "US",
  tel: "+16172535702",
  email: "timbl@w3.org",
};

const TEST_ADDRESS_2 = {
  "street-address": "Some Address",
  country: "US",
};

const TEST_ADDRESS_3 = {
  "street-address": "Other Address",
  "postal-code": "12345",
};

const TEST_ADDRESS_4 = {
  "given-name": "Timothy",
  "family-name": "Berners-Lee",
  organization: "World Wide Web Consortium",
  "street-address": "32 Vassar Street\nMIT Room 32-G524",
  country: "US",
  email: "timbl@w3.org",
};

const TEST_ADDRESS_5 = {
  tel: "+16172535702",
};

const TEST_ADDRESS_CA_1 = {
  "given-name": "John",
  "additional-name": "R.",
  "family-name": "Smith",
  organization: "Mozilla",
  "street-address": "163 W Hastings\nSuite 209",
  "address-level2": "Vancouver",
  "address-level1": "BC",
  "postal-code": "V6B 1H5",
  country: "CA",
  tel: "+17787851540",
  email: "timbl@w3.org",
};

const TEST_ADDRESS_DE_1 = {
  "given-name": "John",
  "additional-name": "R.",
  "family-name": "Smith",
  organization: "Mozilla",
  "street-address":
    "Geb\u00E4ude 3, 4. Obergeschoss\nSchlesische Stra\u00DFe 27",
  "address-level2": "Berlin",
  "postal-code": "10997",
  country: "DE",
  tel: "+4930983333000",
  email: "timbl@w3.org",
};

const TEST_ADDRESS_IE_1 = {
  "given-name": "Bob",
  "additional-name": "Z.",
  "family-name": "Builder",
  organization: "Best Co.",
  "street-address": "123 Kilkenny St.",
  "address-level3": "Some Townland",
  "address-level2": "Dublin",
  "address-level1": "Co. Dublin",
  "postal-code": "A65 F4E2",
  country: "IE",
  tel: "+13534564947391",
  email: "ie@example.com",
};

const TEST_CREDIT_CARD_1 = {
  "cc-name": "John Doe",
  "cc-number": "4111111111111111",
  "cc-exp-month": 4,
  "cc-exp-year": new Date().getFullYear(),
  "cc-type": "visa",
};

const TEST_CREDIT_CARD_2 = {
  "cc-name": "Timothy Berners-Lee",
  "cc-number": "4929001587121045",
  "cc-exp-month": 12,
  "cc-exp-year": new Date().getFullYear() + 10,
  "cc-type": "visa",
};

const TEST_CREDIT_CARD_3 = {
  "cc-number": "5103059495477870",
  "cc-exp-month": 1,
  "cc-exp-year": 2000,
  "cc-type": "mastercard",
};

const TEST_CREDIT_CARD_4 = {
  "cc-number": "5105105105105100",
  "cc-type": "mastercard",
};

const TEST_CREDIT_CARD_5 = {
  "cc-name": "Chris P. Bacon",
  "cc-number": "4012888888881881",
  "cc-type": "visa",
};

const MAIN_BUTTON = "button";
const SECONDARY_BUTTON = "secondaryButton";
const MENU_BUTTON = "menubutton";

/**
 * Collection of timeouts that are used to ensure something should not happen.
 */
const TIMEOUT_ENSURE_PROFILE_NOT_SAVED = 1000;
const TIMEOUT_ENSURE_CC_EDIT_DIALOG_NOT_CLOSED = 500;
const TIMEOUT_ENSURE_AUTOCOMPLETE_NOT_SHOWN = 1000;

function getDisplayedPopupItems(
  browser,
  selector = ".autocomplete-richlistitem"
) {
  info("getDisplayedPopupItems");
  const {
    autoCompletePopup: { richlistbox: itemsBox },
  } = browser;
  const listItemElems = itemsBox.querySelectorAll(selector);

  return [...listItemElems].filter(
    item => item.getAttribute("collapsed") != "true"
  );
}

async function sleep(ms = 500) {
  await new Promise(resolve => setTimeout(resolve, ms));
}

async function ensureNoAutocompletePopup(browser) {
  await new Promise(resolve =>
    setTimeout(resolve, TIMEOUT_ENSURE_AUTOCOMPLETE_NOT_SHOWN)
  );
  const items = getDisplayedPopupItems(browser);
  ok(!items.length, "Should not found autocomplete items");
}

/**
 * Wait for "formautofill-storage-changed" events
 *
 * @param {Array<string>} eventTypes
 *        eventType must be one of the following:
 *        `add`, `update`, `remove`, `notifyUsed`, `removeAll`, `reconcile`
 *
 * @returns {Promise} resolves when all events are received
 */
async function waitForStorageChangedEvents(...eventTypes) {
  return Promise.all(
    eventTypes.map(type =>
      TestUtils.topicObserved(
        "formautofill-storage-changed",
        (subject, data) => {
          return data == type;
        }
      )
    )
  );
}

/**
 * Wait until the element found matches the expected autofill value
 *
 * @param {Object} target
 *        The target in which to run the task.
 * @param {string} selector
 *        A selector used to query the element.
 * @param {string} value
 *        The expected autofilling value for the element
 */
async function waitForAutofill(target, selector, value) {
  await SpecialPowers.spawn(target, [selector, value], async function(
    selector,
    val
  ) {
    await ContentTaskUtils.waitForCondition(() => {
      let element = content.document.querySelector(selector);
      return element.value == val;
    }, "Autofill never fills");
  });
}

/**
 * Waits for the subDialog to be loaded
 *
 * @param {Window} win The window of the dialog
 * @param {string} dialogUrl The url of the dialog that we are waiting for
 *
 * @returns {Promise} resolves when the sub dialog is loaded
 */
function waitForSubDialogLoad(win, dialogUrl) {
  return new Promise((resolve, reject) => {
    win.gSubDialog._dialogStack.addEventListener(
      "dialogopen",
      async function dialogopen(evt) {
        let cwin = evt.detail.dialog._frame.contentWindow;
        if (cwin.location != dialogUrl) {
          return;
        }
        content.gSubDialog._dialogStack.removeEventListener(
          "dialogopen",
          dialogopen
        );

        resolve(cwin);
      }
    );
  });
}

/**
 * Use this function when you want to update the value of elements in
 * a form and then submit the form. This function makes sure the form
 * is "identified" (`identifyAutofillFields` is called) before submitting
 * the form.
 * This is guaranteed by first focusing on an element in the form to trigger
 * the 'FormAutofill:FieldsIdentified' message.
 *
 * @param {Object} target
 *        The target in which to run the task.
 * @param {Object} args
 * @param {string} args.focusSelector
 *        A selector used to query the element to be focused
 * @param {string} args.formId
 *        The id of the form to be updated. This function uses "form" if
 *        this argument is not present
 * @param {string} args.formSelector
 *        A selector used to query the form element
 * @param {Object} args.newValues
 *        Elements to be updated. Key is the element selector, value is the
 *        new value of the element.
 *
 * @param {boolean} submit
 *        Set to true to submit the form after the task is done, false otherwise.
 */
async function focusUpdateSubmitForm(target, args, submit = true) {
  let fieldsIdentifiedPromiseResolver;
  let fieldsIdentifiedObserver = {
    fieldsIdentified() {
      FormAutofillParent.removeMessageObserver(fieldsIdentifiedObserver);
      fieldsIdentifiedPromiseResolver();
    },
  };

  let fieldsIdentifiedPromise = new Promise(resolve => {
    fieldsIdentifiedPromiseResolver = resolve;
    FormAutofillParent.addMessageObserver(fieldsIdentifiedObserver);
  });

  let alreadyFocused = await SpecialPowers.spawn(target, [args], obj => {
    let focused = false;

    let form;
    if (obj.formSelector) {
      form = content.document.querySelector(obj.formSelector);
    } else {
      form = content.document.getElementById(obj.formId ?? "form");
    }
    let element = form.querySelector(obj.focusSelector);
    if (element != content.document.activeElement) {
      info(`focus on element (id=${element.id})`);
      element.focus();
    } else {
      focused = true;
    }

    for (const [selector, value] of Object.entries(obj.newValues)) {
      element = form.querySelector(selector);
      if (content.HTMLInputElement.isInstance(element)) {
        element.setUserInput(value);
      } else {
        element.value = value;
      }
    }

    return focused;
  });

  if (alreadyFocused) {
    // If the element is already focused, assume the FieldsIdentified message
    // was sent before.
    fieldsIdentifiedPromiseResolver();
  }

  await fieldsIdentifiedPromise;

  if (submit) {
    await SpecialPowers.spawn(target, [args], obj => {
      let form;
      if (obj.formSelector) {
        form = content.document.querySelector(obj.formSelector);
      } else {
        form = content.document.getElementById(obj.formId ?? "form");
      }
      info(`submit form (id=${form.id})`);
      form.querySelector("input[type=submit]").click();
    });
  }
}

async function focusAndWaitForFieldsIdentified(browserOrContext, selector) {
  info("expecting the target input being focused and identified");
  /* eslint no-shadow: ["error", { "allow": ["selector", "previouslyFocused", "previouslyIdentified"] }] */

  // If the input is previously focused, no more notifications will be
  // sent as the notification goes along with focus event.
  let fieldsIdentifiedPromiseResolver;
  let fieldsIdentifiedObserver = {
    fieldsIdentified() {
      fieldsIdentifiedPromiseResolver();
    },
  };

  let fieldsIdentifiedPromise = new Promise(resolve => {
    fieldsIdentifiedPromiseResolver = resolve;
    FormAutofillParent.addMessageObserver(fieldsIdentifiedObserver);
  });

  const { previouslyFocused, previouslyIdentified } = await SpecialPowers.spawn(
    browserOrContext,
    [selector],
    async function(selector) {
      const { FormLikeFactory } = ChromeUtils.import(
        "resource://gre/modules/FormLikeFactory.jsm"
      );
      const input = content.document.querySelector(selector);
      const rootElement = FormLikeFactory.findRootForField(input);
      const previouslyFocused = content.document.activeElement == input;
      const previouslyIdentified = rootElement.hasAttribute(
        "test-formautofill-identified"
      );

      input.focus();

      return { previouslyFocused, previouslyIdentified };
    }
  );

  // Only wait for the fields identified notification if the
  // focus was not previously assigned to the input.
  if (previouslyFocused) {
    fieldsIdentifiedPromiseResolver();
  } else {
    info("!previouslyFocused");
  }

  // If a browsing context was supplied, focus its parent frame as well.
  if (
    BrowsingContext.isInstance(browserOrContext) &&
    browserOrContext.parent != browserOrContext
  ) {
    await SpecialPowers.spawn(
      browserOrContext.parent,
      [browserOrContext],
      async function(browsingContext) {
        browsingContext.embedderElement.focus();
      }
    );
  }

  if (previouslyIdentified) {
    info("previouslyIdentified");
    FormAutofillParent.removeMessageObserver(fieldsIdentifiedObserver);
    return;
  }

  // Wait 500ms to ensure that "markAsAutofillField" is completely finished.
  await fieldsIdentifiedPromise;
  info("FieldsIdentified");
  FormAutofillParent.removeMessageObserver(fieldsIdentifiedObserver);

  await sleep();
  await SpecialPowers.spawn(browserOrContext, [], async function() {
    const { FormLikeFactory } = ChromeUtils.import(
      "resource://gre/modules/FormLikeFactory.jsm"
    );
    FormLikeFactory.findRootForField(
      content.document.activeElement
    ).setAttribute("test-formautofill-identified", "true");
  });
}

/**
 * Run the task and wait until the autocomplete popup is opened.
 *
 * @param {Object} browser A xul:browser.
 * @param {Function} taskFn Task that will trigger the autocomplete popup
 */
async function runAndWaitForAutocompletePopupOpen(browser, taskFn) {
  info("runAndWaitForAutocompletePopupOpen");
  let popupShown = BrowserTestUtils.waitForPopupEvent(
    browser.autoCompletePopup,
    "shown"
  );

  // Run the task will open the autocomplete popup
  await taskFn();

  await popupShown;
  await BrowserTestUtils.waitForMutationCondition(
    browser.autoCompletePopup.richlistbox,
    { childList: true, subtree: true, attributes: true },
    () => {
      const listItemElems = getDisplayedPopupItems(browser);
      return (
        !![...listItemElems].length &&
        [...listItemElems].every(item => {
          return (
            (item.getAttribute("originaltype") == "autofill-profile" ||
              item.getAttribute("originaltype") == "autofill-insecureWarning" ||
              item.getAttribute("originaltype") == "autofill-clear-button" ||
              item.getAttribute("originaltype") == "autofill-footer") &&
            item.hasAttribute("formautofillattached")
          );
        })
      );
    }
  );
}

async function waitForPopupEnabled(browser) {
  const {
    autoCompletePopup: { richlistbox: itemsBox },
  } = browser;
  info("Wait for list elements to become enabled");
  await BrowserTestUtils.waitForMutationCondition(
    itemsBox,
    { subtree: true, attributes: true, attributeFilter: ["disabled"] },
    () => !itemsBox.querySelectorAll(".autocomplete-richlistitem")[0].disabled
  );
}

// Wait for the popup state change notification to happen in a child process.
function waitPopupStateInChild(bc, messageName) {
  return SpecialPowers.spawn(bc, [messageName], expectedMessage => {
    return new Promise(resolve => {
      const { AutoCompleteChild } = ChromeUtils.import(
        "resource://gre/actors/AutoCompleteChild.jsm"
      );

      let listener = {
        popupStateChanged: name => {
          if (name != expectedMessage) {
            info("Expected " + expectedMessage + " but received " + name);
            return;
          }

          AutoCompleteChild.removePopupStateListener(listener);
          resolve();
        },
      };
      AutoCompleteChild.addPopupStateListener(listener);
    });
  });
}

async function openPopupOn(browser, selector) {
  let childNotifiedPromise = waitPopupStateInChild(
    browser,
    "FormAutoComplete:PopupOpened"
  );
  await SimpleTest.promiseFocus(browser);

  await runAndWaitForAutocompletePopupOpen(browser, async () => {
    await focusAndWaitForFieldsIdentified(browser, selector);
    if (!selector.includes("cc-")) {
      info(`openPopupOn: before VK_DOWN on ${selector}`);
      await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);
    }
  });

  await childNotifiedPromise;
}

async function openPopupOnSubframe(browser, frameBrowsingContext, selector) {
  let childNotifiedPromise = waitPopupStateInChild(
    frameBrowsingContext,
    "FormAutoComplete:PopupOpened"
  );

  await SimpleTest.promiseFocus(browser);

  await runAndWaitForAutocompletePopupOpen(browser, async () => {
    await focusAndWaitForFieldsIdentified(frameBrowsingContext, selector);
    if (!selector.includes("cc-")) {
      info(`openPopupOnSubframe: before VK_DOWN on ${selector}`);
      await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, frameBrowsingContext);
    }
  });

  await childNotifiedPromise;
}

async function closePopup(browser) {
  // Return if the popup isn't open.
  if (!browser.autoCompletePopup.popupOpen) {
    return;
  }

  let childNotifiedPromise = waitPopupStateInChild(
    browser,
    "FormAutoComplete:PopupClosed"
  );
  let popupClosePromise = BrowserTestUtils.waitForPopupEvent(
    browser.autoCompletePopup,
    "hidden"
  );

  await SpecialPowers.spawn(browser, [], async function() {
    content.document.activeElement.blur();
  });

  await popupClosePromise;
  await childNotifiedPromise;
}

async function closePopupForSubframe(browser, frameBrowsingContext) {
  let childNotifiedPromise = waitPopupStateInChild(
    browser,
    "FormAutoComplete:PopupClosed"
  );

  let popupClosePromise = BrowserTestUtils.waitForPopupEvent(
    browser.autoCompletePopup,
    "hidden"
  );

  await SpecialPowers.spawn(frameBrowsingContext, [], async function() {
    content.document.activeElement.blur();
  });

  await popupClosePromise;
  await childNotifiedPromise;
}

function emulateMessageToBrowser(name, data) {
  let actor = gBrowser.selectedBrowser.browsingContext.currentWindowGlobal.getActor(
    "FormAutofill"
  );
  return actor.receiveMessage({ name, data });
}

function getRecords(data) {
  info(`expecting record retrievals: ${data.collectionName}`);
  return emulateMessageToBrowser("FormAutofill:GetRecords", data);
}

function getAddresses() {
  return getRecords({ collectionName: "addresses" });
}

function getCreditCards() {
  return getRecords({ collectionName: "creditCards" });
}

async function saveAddress(address) {
  info("expecting address saved");
  let observePromise = TestUtils.topicObserved("formautofill-storage-changed");
  await emulateMessageToBrowser("FormAutofill:SaveAddress", { address });
  await observePromise;
}

async function saveCreditCard(creditcard) {
  info("expecting credit card saved");
  let creditcardClone = Object.assign({}, creditcard);
  let observePromise = TestUtils.topicObserved("formautofill-storage-changed");
  await emulateMessageToBrowser("FormAutofill:SaveCreditCard", {
    creditcard: creditcardClone,
  });
  await observePromise;
}

async function removeAddresses(guids) {
  info("expecting address removed");
  let observePromise = TestUtils.topicObserved("formautofill-storage-changed");
  await emulateMessageToBrowser("FormAutofill:RemoveAddresses", { guids });
  await observePromise;
}

async function removeCreditCards(guids) {
  info("expecting credit card removed");
  let observePromise = TestUtils.topicObserved("formautofill-storage-changed");
  await emulateMessageToBrowser("FormAutofill:RemoveCreditCards", { guids });
  await observePromise;
}

function getNotification(index = 0) {
  let notifications = PopupNotifications.panel.childNodes;
  ok(!!notifications.length, "at least one notification displayed");
  ok(true, notifications.length + " notification(s)");
  return notifications[index];
}

function waitForPopupShown() {
  return BrowserTestUtils.waitForEvent(PopupNotifications.panel, "popupshown");
}

/**
 * Clicks the popup notification button and wait for popup hidden.
 *
 * @param {string} button The button type in popup notification.
 * @param {number} index The action's index in menu list.
 */
async function clickDoorhangerButton(button, index) {
  let popuphidden = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popuphidden"
  );

  if (button == MAIN_BUTTON || button == SECONDARY_BUTTON) {
    EventUtils.synthesizeMouseAtCenter(getNotification()[button], {});
  } else if (button == MENU_BUTTON) {
    // Click the dropmarker arrow and wait for the menu to show up.
    info("expecting notification menu button present");
    await BrowserTestUtils.waitForCondition(() => getNotification().menubutton);
    await sleep(2000); // menubutton needs extra time for binding
    let notification = getNotification();
    ok(notification.menubutton, "notification menupopup displayed");
    let dropdownPromise = BrowserTestUtils.waitForEvent(
      notification.menupopup,
      "popupshown"
    );
    await EventUtils.synthesizeMouseAtCenter(notification.menubutton, {});
    info("expecting notification popup show up");
    await dropdownPromise;

    let actionMenuItem = notification.querySelectorAll("menuitem")[index];
    await EventUtils.synthesizeMouseAtCenter(actionMenuItem, {});
  }
  info("expecting notification popup hidden");
  await popuphidden;
}

function getDoorhangerCheckbox() {
  return getNotification().checkbox;
}

function getDoorhangerButton(button) {
  return getNotification()[button];
}

/**
 * Removes all addresses and credit cards from storage.
 *
 * **NOTE: If you add or update a record in a test, then you must wait for the
 * respective storage event to fire before calling this function.**
 * This is because this function doesn't guarantee that a record that
 * is about to be added or update will also be removed,
 * since the add or update is triggered by an asynchronous call.
 *
 * @see waitForStorageChangedEvents for more details about storage events to wait for
 */
async function removeAllRecords() {
  let addresses = await getAddresses();
  if (addresses.length) {
    await removeAddresses(addresses.map(address => address.guid));
  }
  let creditCards = await getCreditCards();
  if (creditCards.length) {
    await removeCreditCards(creditCards.map(cc => cc.guid));
  }
}

async function waitForFocusAndFormReady(win) {
  return Promise.all([
    new Promise(resolve => waitForFocus(resolve, win)),
    BrowserTestUtils.waitForEvent(win, "FormReady"),
  ]);
}

// Verify that the warning in the autocomplete popup has the expected text.
async function expectWarningText(browser, expectedText) {
  const {
    autoCompletePopup: { richlistbox: itemsBox },
  } = browser;
  let warningBox = itemsBox.querySelector(
    ".autocomplete-richlistitem:last-child"
  );

  while (warningBox.collapsed) {
    warningBox = warningBox.previousSibling;
  }
  warningBox = warningBox._warningTextBox;

  await BrowserTestUtils.waitForMutationCondition(
    warningBox,
    { childList: true, characterData: true },
    () => warningBox.textContent == expectedText
  );
  ok(true, `Got expected warning text: ${expectedText}`);
}

async function testDialog(url, testFn, arg = undefined) {
  // Skip this step for test cards that lack an encrypted
  // number since they will fail to decrypt.
  if (
    url == EDIT_CREDIT_CARD_DIALOG_URL &&
    arg &&
    arg.record &&
    arg.record["cc-number-encrypted"]
  ) {
    arg.record = Object.assign({}, arg.record, {
      "cc-number": await OSKeyStore.decrypt(arg.record["cc-number-encrypted"]),
    });
  }
  let win = window.openDialog(url, null, "width=600,height=600", arg);
  await waitForFocusAndFormReady(win);
  let unloadPromise = BrowserTestUtils.waitForEvent(win, "unload");
  await testFn(win);
  return unloadPromise;
}

/**
 * Initializes the test storage for a task.
 *
 * @param {...Object} items Can either be credit card or address objects
 */
async function setStorage(...items) {
  for (let item of items) {
    if (item["cc-number"]) {
      await saveCreditCard(item);
    } else {
      await saveAddress(item);
    }
  }
}

add_setup(function() {
  OSKeyStoreTestUtils.setup();
});

registerCleanupFunction(async () => {
  await removeAllRecords();
  await OSKeyStoreTestUtils.cleanup();
});
