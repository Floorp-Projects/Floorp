"use strict";

const { OSKeyStore } = ChromeUtils.importESModule(
  "resource://gre/modules/OSKeyStore.sys.mjs"
);
const { OSKeyStoreTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/OSKeyStoreTestUtils.sys.mjs"
);

const { FormAutofillParent } = ChromeUtils.importESModule(
  "resource://autofill/FormAutofillParent.sys.mjs"
);

const { AutofillDoorhanger, AddressEditDoorhanger, AddressSaveDoorhanger } =
  ChromeUtils.importESModule(
    "resource://autofill/FormAutofillPrompter.sys.mjs"
  );

const { FormAutofillNameUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/shared/FormAutofillNameUtils.sys.mjs"
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
const ADDRESS_FORM_URL =
  "https://example.org" +
  HTTP_TEST_PATH +
  "address/autocomplete_address_basic.html";
const ADDRESS_FORM_WITHOUT_AUTOCOMPLETE_URL =
  "https://example.org" +
  HTTP_TEST_PATH +
  "address/without_autocomplete_address_basic.html";
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
const CREDITCARD_FORM_WITHOUT_AUTOCOMPLETE_URL =
  "https://example.org" +
  HTTP_TEST_PATH +
  "creditCard/without_autocomplete_creditcard_basic.html";
const EMPTY_URL = "https://example.org" + HTTP_TEST_PATH + "empty.html";

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
  "given-name": "Anonymouse",
  "street-address": "Some Address",
  country: "US",
};

const TEST_ADDRESS_3 = {
  "given-name": "John",
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

// TODO: Number of field less than AUTOFILL_FIELDS_THRESHOLD
//       need to confirm whether this is intentional
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
};

const TEST_CREDIT_CARD_2 = {
  "cc-name": "Timothy Berners-Lee",
  "cc-number": "4929001587121045",
  "cc-exp-month": 12,
  "cc-exp-year": new Date().getFullYear() + 10,
};

const TEST_CREDIT_CARD_3 = {
  "cc-number": "5103059495477870",
  "cc-exp-month": 1,
  "cc-exp-year": 2000,
};

const TEST_CREDIT_CARD_4 = {
  "cc-number": "5105105105105100",
};

const TEST_CREDIT_CARD_5 = {
  "cc-name": "Chris P. Bacon",
  "cc-number": "4012888888881881",
};

const MAIN_BUTTON = "button";
const SECONDARY_BUTTON = "secondaryButton";
const MENU_BUTTON = "menubutton";
const EDIT_ADDRESS_BUTTON = "edit";
const ADDRESS_MENU_BUTTON = "addressMenuButton";
const ADDRESS_MENU_LEARN_MORE = "learnMore";
const ADDRESS_MENU_PREFENCE = "preference";

/**
 * Collection of timeouts that are used to ensure something should not happen.
 */
const TIMEOUT_ENSURE_PROFILE_NOT_SAVED = 1000;
const TIMEOUT_ENSURE_CC_DIALOG_NOT_CLOSED = 500;
const TIMEOUT_ENSURE_AUTOCOMPLETE_NOT_SHOWN = 1000;
const TIMEOUT_ENSURE_DOORHANGER_NOT_SHOWN = 1000;

async function ensureCreditCardDialogNotClosed(win) {
  const unloadHandler = () => {
    ok(false, "Credit card dialog shouldn't be closed");
  };
  win.addEventListener("unload", unloadHandler);
  await new Promise(resolve =>
    setTimeout(resolve, TIMEOUT_ENSURE_CC_DIALOG_NOT_CLOSED)
  );
  win.removeEventListener("unload", unloadHandler);
}

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
  ok(!items.length, "Should not find autocomplete items");
}

async function ensureNoDoorhanger(browser) {
  await new Promise(resolve =>
    setTimeout(resolve, TIMEOUT_ENSURE_DOORHANGER_NOT_SHOWN)
  );

  let notifications = PopupNotifications.panel.childNodes;
  ok(!notifications.length, "Should not find a doorhanger");
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
 * @param {object} target
 *        The target in which to run the task.
 * @param {string} selector
 *        A selector used to query the element.
 * @param {string} value
 *        The expected autofilling value for the element
 */
async function waitForAutofill(target, selector, value) {
  await SpecialPowers.spawn(
    target,
    [selector, value],
    async function (selector, val) {
      await ContentTaskUtils.waitForCondition(() => {
        let element = content.document.querySelector(selector);
        return element.value == val;
      }, "Autofill never fills");
    }
  );
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
 * @param {object} target
 *        The target in which to run the task.
 * @param {object} args
 * @param {string} args.focusSelector
 *        A selector used to query the element to be focused
 * @param {string} args.formId
 *        The id of the form to be updated. This function uses "form" if
 *        this argument is not present
 * @param {string} args.formSelector
 *        A selector used to query the form element
 * @param {object} args.newValues
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
    async function (selector) {
      const { FormLikeFactory } = ChromeUtils.importESModule(
        "resource://gre/modules/FormLikeFactory.sys.mjs"
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
      async function (browsingContext) {
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
  await SpecialPowers.spawn(browserOrContext, [], async function () {
    const { FormLikeFactory } = ChromeUtils.importESModule(
      "resource://gre/modules/FormLikeFactory.sys.mjs"
    );
    FormLikeFactory.findRootForField(
      content.document.activeElement
    ).setAttribute("test-formautofill-identified", "true");
  });
}

/**
 * Run the task and wait until the autocomplete popup is opened.
 *
 * @param {object} browser A xul:browser.
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
      const { AutoCompleteChild } = ChromeUtils.importESModule(
        "resource://gre/actors/AutoCompleteChild.sys.mjs"
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

  await SpecialPowers.spawn(browser, [], async function () {
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

  await SpecialPowers.spawn(frameBrowsingContext, [], async function () {
    content.document.activeElement.blur();
  });

  await popupClosePromise;
  await childNotifiedPromise;
}

function emulateMessageToBrowser(name, data) {
  let actor =
    gBrowser.selectedBrowser.browsingContext.currentWindowGlobal.getActor(
      "FormAutofill"
    );
  return actor.receiveMessage({ name, data });
}

function getRecords(data) {
  info(`expecting record retrievals: ${data.collectionName}`);
  return emulateMessageToBrowser("FormAutofill:GetRecords", data).then(
    result => result.records
  );
}

function getAddresses() {
  return getRecords({ collectionName: "addresses" });
}

async function ensureNoAddressSaved() {
  await new Promise(resolve =>
    setTimeout(resolve, TIMEOUT_ENSURE_PROFILE_NOT_SAVED)
  );
  const addresses = await getAddresses();
  is(addresses.length, 0, "No address was saved");
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
 * @param {string} buttonType The button type in popup notification.
 * @param {number} index The action's index in menu list.
 */
async function clickDoorhangerButton(buttonType, index = 0) {
  let popuphidden = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popuphidden"
  );

  let button;
  if (buttonType == MAIN_BUTTON || buttonType == SECONDARY_BUTTON) {
    button = getNotification()[buttonType];
  } else if (buttonType == MENU_BUTTON) {
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

    notification.menubutton.click();
    info("expecting notification popup show up");
    await dropdownPromise;

    button = notification.querySelectorAll("menuitem")[index];
  }

  button.click();
  info("expecting notification popup hidden");
  await popuphidden;
}

async function clickAddressDoorhangerButton(buttonType, subType) {
  const notification = getNotification();
  let button;
  if (buttonType == EDIT_ADDRESS_BUTTON) {
    button = AddressSaveDoorhanger.editButton(notification);
  } else if (buttonType == ADDRESS_MENU_BUTTON) {
    const menu = AutofillDoorhanger.menuButton(notification);
    const promise = BrowserTestUtils.waitForEvent(menu.menupopup, "popupshown");
    menu.click();
    await promise;
    if (subType == ADDRESS_MENU_PREFENCE) {
      button = AutofillDoorhanger.preferenceButton(notification);
    } else if (subType == ADDRESS_MENU_LEARN_MORE) {
      button = AutofillDoorhanger.learnMoreButton(notification);
    }
  } else {
    await clickDoorhangerButton(buttonType);
    return;
  }

  EventUtils.synthesizeMouseAtCenter(button, {});
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
 * @param {...object} items Can either be credit card or address objects
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

function verifySectionAutofillResult(sections, expectedSectionsInfo) {
  sections.forEach((section, index) => {
    const expectedSection = expectedSectionsInfo[index];

    const fieldDetails = section.fieldDetails;
    const expectedFieldDetails = expectedSection.fields;

    info(`verify autofill section[${index}]`);

    fieldDetails.forEach((field, fieldIndex) => {
      const expeceted = expectedFieldDetails[fieldIndex];

      Assert.equal(
        field.element.value,
        expeceted.autofill ?? "",
        `Autofilled value for element(id=${field.element.id}, field name=${field.fieldName}) should be equal`
      );
    });
  });
}

function verifySectionFieldDetails(sections, expectedSectionsInfo) {
  sections.forEach((section, index) => {
    const expectedSection = expectedSectionsInfo[index];

    const fieldDetails = section.fieldDetails;
    const expectedFieldDetails = expectedSection.fields;

    info(`section[${index}] ${expectedSection.description ?? ""}:`);
    info(`FieldName Prediction Results: ${fieldDetails.map(i => i.fieldName)}`);
    info(
      `FieldName Expected Results:   ${expectedFieldDetails.map(
        detail => detail.fieldName
      )}`
    );
    Assert.equal(
      fieldDetails.length,
      expectedFieldDetails.length,
      `Expected field count.`
    );

    fieldDetails.forEach((field, fieldIndex) => {
      const expectedFieldDetail = expectedFieldDetails[fieldIndex];

      const expected = {
        ...{
          reason: "autocomplete",
          section: "",
          contactType: "",
          addressType: "",
        },
        ...expectedSection.default,
        ...expectedFieldDetail,
      };

      const keys = new Set([...Object.keys(field), ...Object.keys(expected)]);
      [
        "identifier",
        "autofill",
        "elementWeakRef",
        "confidence",
        "part",
      ].forEach(k => keys.delete(k));

      for (const key of keys) {
        const expectedValue = expected[key];
        const actualValue = field[key];
        Assert.equal(
          actualValue,
          expectedValue,
          `${key} should be equal, expect ${expectedValue}, got ${actualValue}`
        );
      }
    });

    Assert.equal(
      section.isValidSection(),
      !expectedSection.invalid,
      `Should be an ${expectedSection.invalid ? "invalid" : "valid"} section`
    );
  });
}

/**
 * Discards all recorded Glean telemetry in parent and child processes
 * and resets FOG and the Glean SDK.
 *
 * @param {boolean} onlyInParent Whether we only discard the metric data in the parent process
 *
 * Since the current method Services.fog.testResetFOG only discards metrics recorded in the parent process,
 * we would like to keep this option in our method as well.
 */
async function clearGleanTelemetry(onlyInParent = false) {
  if (!onlyInParent) {
    await Services.fog.testFlushAllChildren();
  }
  Services.fog.testResetFOG();
}

/**
 * Runs heuristics test for form autofill on given patterns.
 *
 * @param {Array<object>} patterns - An array of test patterns to run the heuristics test on.
 * @param {string} pattern.description - Description of this heuristic test
 * @param {string} pattern.fixurePath - The path of the test document
 * @param {string} pattern.fixureData - Test document by string. Use either fixurePath or fixtureData.
 * @param {object} pattern.profile - The profile to autofill. This is required only when running autofill test
 * @param {Array}  pattern.expectedResult - The expected result of this heuristic test. See below for detailed explanation
 *
 * @param {string} [fixturePathPrefix=""] - The prefix to the path of fixture files.
 * @param {object} [options={ testAutofill: false }] - An options object containing additional configuration for running the test.
 * @param {boolean} [options.testAutofill=false] - A boolean indicating whether to run the test for autofill or not.
 * @returns {Promise} A promise that resolves when all the tests are completed.
 *
 * The `patterns.expectedResult` array contains test data for different address or credit card sections.
 * Each section in the array is represented by an object and can include the following properties:
 * - description (optional): A string describing the section, primarily used for debugging purposes.
 * - default (optional): An object that sets the default values for all the fields within this section.
 *            The default object contains the same keys as the individual field objects.
 * - fields: An array of field details (class FieldDetails) within the section.
 *
 * Each field object can have the following keys:
 * - fieldName: The name of the field (e.g., "street-name", "cc-name" or "cc-number").
 * - reason: The reason for the field value (e.g., "autocomplete", "regex-heuristic" or "fathom").
 * - section: The section to which the field belongs (e.g., "billing", "shipping").
 * - part: The part of the field.
 * - contactType: The contact type of the field.
 * - addressType: The address type of the field.
 * - autofill: Set the expected autofill value when running autofill test
 *
 * For more information on the field object properties, refer to the FieldDetails class.
 *
 * Example test data:
 * add_heuristic_tests(
 * [{
 *   description: "first test pattern",
 *   fixuturePath: "autocomplete_off.html",
 *   profile: {organization: "Mozilla", country: "US", tel: "123"},
 *   expectedResult: [
 *   {
 *     description: "First section"
 *     fields: [
 *       { fieldName: "organization", reason: "autocomplete", autofill: "Mozilla" },
 *       { fieldName: "country", reason: "regex-heuristic", autofill: "US" },
 *       { fieldName: "tel", reason: "regex-heuristic", autofill: "123" },
 *     ]
 *   },
 *   {
 *     default: {
 *       reason: "regex-heuristic",
 *       section: "billing",
 *     },
 *     fields: [
 *       { fieldName: "cc-number", reason: "fathom" },
 *       { fieldName: "cc-nane" },
 *       { fieldName: "cc-exp" },
 *     ],
 *    }],
 *  },
 *  {
 *    // second test pattern //
 *  }
 * ],
 * "/fixturepath",
 * {testAutofill: true}  // test options
 * )
 */

async function add_heuristic_tests(
  patterns,
  fixturePathPrefix = "",
  options = { testAutofill: false }
) {
  async function runTest(testPattern) {
    const TEST_URL = testPattern.fixtureData
      ? `data:text/html,${testPattern.fixtureData}`
      : `${BASE_URL}../${fixturePathPrefix}${testPattern.fixturePath}`;

    if (testPattern.fixtureData) {
      info(`Starting test with fixture data`);
    } else {
      info(`Starting test fixture: ${testPattern.fixturePath ?? ""}`);
    }

    if (testPattern.description) {
      info(`Test "${testPattern.description}"`);
    }

    if (testPattern.prefs) {
      await SpecialPowers.pushPrefEnv({
        set: testPattern.prefs,
      });
    }

    await BrowserTestUtils.withNewTab(TEST_URL, async browser => {
      await SpecialPowers.spawn(
        browser,
        [
          {
            testPattern,
            verifySection: verifySectionFieldDetails.toString(),
            verifyAutofill: options.testAutofill
              ? verifySectionAutofillResult.toString()
              : null,
          },
        ],
        async obj => {
          const { FormLikeFactory } = ChromeUtils.importESModule(
            "resource://gre/modules/FormLikeFactory.sys.mjs"
          );
          const { FormAutofillHandler } = ChromeUtils.importESModule(
            "resource://gre/modules/shared/FormAutofillHandler.sys.mjs"
          );

          const elements = Array.from(
            content.document.querySelectorAll("input, select")
          );

          // Bug 1834768. We should simulate user behavior instead of
          // using internal APIs.
          const forms = elements.reduce((acc, element) => {
            const formLike = FormLikeFactory.createFromField(element);
            if (!acc.some(form => form.rootElement === formLike.rootElement)) {
              acc.push(formLike);
            }
            return acc;
          }, []);

          const sections = forms.flatMap(form => {
            const handler = new FormAutofillHandler(form);
            handler.collectFormFields(false /* ignoreInvalid */);
            return handler.sections;
          });

          Assert.equal(
            sections.length,
            obj.testPattern.expectedResult.length,
            "Expected section count."
          );

          // eslint-disable-next-line no-eval
          let verify = eval(`(() => {return (${obj.verifySection});})();`);
          verify(sections, obj.testPattern.expectedResult);

          if (obj.verifyAutofill) {
            for (const section of sections) {
              if (!section.isValidSection()) {
                continue;
              }

              section.focusedInput = section.fieldDetails[0].element;
              await section.autofillFields(
                section.getAdaptedProfiles([obj.testPattern.profile])[0]
              );
            }

            // eslint-disable-next-line no-eval
            verify = eval(`(() => {return (${obj.verifyAutofill});})();`);
            verify(sections, obj.testPattern.expectedResult);
          }
        }
      );
    });

    if (testPattern.prefs) {
      await SpecialPowers.popPrefEnv();
    }
  }

  patterns.forEach(testPattern => {
    add_task(() => runTest(testPattern));
  });
}

async function add_autofill_heuristic_tests(patterns, fixturePathPrefix = "") {
  add_heuristic_tests(patterns, fixturePathPrefix, { testAutofill: true });
}

function fillEditDoorhanger(record) {
  const notification = getNotification();

  for (const [key, value] of Object.entries(record)) {
    const id = AddressEditDoorhanger.getInputId(key);
    const element = notification.querySelector(`#${id}`);
    element.value = value;
  }
}

// TODO: This function should be removed. We should make normalizeFields in
// FormAutofillStorageBase.sys.mjs static and using it directly
function normalizeAddressFields(record) {
  let normalized = { ...record };

  if (normalized.name != undefined) {
    let nameParts = FormAutofillNameUtils.splitName(normalized.name);
    normalized["given-name"] = nameParts.given;
    normalized["additional-name"] = nameParts.middle;
    normalized["family-name"] = nameParts.family;
    delete normalized.name;
  }
  return normalized;
}

async function verifyConfirmationHint(
  browser,
  forceClose,
  anchorID = "identity-icon"
) {
  let hintElem = browser.ownerGlobal.ConfirmationHint._panel;
  await BrowserTestUtils.waitForPopupEvent(hintElem, "shown");
  try {
    Assert.equal(hintElem.state, "open", "hint popup is open");
    Assert.ok(
      BrowserTestUtils.is_visible(hintElem.anchorNode),
      "hint anchorNode is visible"
    );
    Assert.equal(
      hintElem.anchorNode.id,
      anchorID,
      "Hint should be anchored on the expected notification icon"
    );
    info("verifyConfirmationHint, hint is shown and has its anchorNode");
    if (forceClose) {
      await closePopup(hintElem);
    } else {
      info("verifyConfirmationHint, assertion ok, wait for poopuphidden");
      await BrowserTestUtils.waitForPopupEvent(hintElem, "hidden");
      info("verifyConfirmationHint, hintElem popup is hidden");
    }
  } catch (ex) {
    Assert.ok(false, "Confirmation hint not shown: " + ex.message);
  } finally {
    info("verifyConfirmationHint promise finalized");
  }
}

async function showAddressDoorhanger(browser, values = null) {
  const defaultValues = {
    "#given-name": "John",
    "#family-name": "Doe",
    "#organization": "Mozilla",
    "#street-address": "123 Sesame Street",
  };

  const onPopupShown = waitForPopupShown();
  const promise = BrowserTestUtils.browserLoaded(browser);
  await focusUpdateSubmitForm(browser, {
    focusSelector: "#given-name",
    newValues: values ?? defaultValues,
  });
  await promise;
  await onPopupShown;
}

add_setup(function () {
  OSKeyStoreTestUtils.setup();
});

registerCleanupFunction(async () => {
  await removeAllRecords();
  await OSKeyStoreTestUtils.cleanup();
});
