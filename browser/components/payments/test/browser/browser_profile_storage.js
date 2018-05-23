/* eslint-disable no-shadow */

"use strict";

const methodData = [PTU.MethodData.basicCard];
const details = PTU.Details.total60USD;

add_task(async function test_initial_state() {
  let onChanged = TestUtils.topicObserved("formautofill-storage-changed",
                                          (subject, data) => data == "add");
  let address1GUID = formAutofillStorage.addresses.add(PTU.Addresses.TimBL);
  await onChanged;

  onChanged = TestUtils.topicObserved("formautofill-storage-changed",
                                      (subject, data) => data == "add");
  let card1GUID = formAutofillStorage.creditCards.add(PTU.BasicCards.JohnDoe);
  await onChanged;

  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: BLANK_PAGE_URL,
  }, async browser => {
    let {win, frame} =
      await setupPaymentDialog(browser, {
        methodData,
        details,
        merchantTaskFn: PTU.ContentTasks.createAndShowRequest,
      }
    );

    await spawnPaymentDialogTask(frame, async function checkInitialStore({
      address1GUID,
      card1GUID,
    }) {
      info("checkInitialStore");
      let contentWin = Cu.waiveXrays(content);
      let {
        savedAddresses,
        savedBasicCards,
      } = contentWin.document.querySelector("payment-dialog").requestStore.getState();

      is(Object.keys(savedAddresses).length, 1, "Initially one savedAddresses");
      is(savedAddresses[address1GUID].name, "Timothy John Berners-Lee", "Check full name");
      is(savedAddresses[address1GUID].guid, address1GUID, "Check address guid matches key");

      is(Object.keys(savedBasicCards).length, 1, "Initially one savedBasicCards");
      is(savedBasicCards[card1GUID]["cc-number"], "************1111", "Check cc-number");
      is(savedBasicCards[card1GUID].guid, card1GUID, "Check card guid matches key");
      is(savedBasicCards[card1GUID].methodName, "basic-card",
         "Check card has a methodName of basic-card");
    }, {
      address1GUID,
      card1GUID,
    });

    let onChanged = TestUtils.topicObserved("formautofill-storage-changed",
                                            (subject, data) => data == "add");
    info("adding an address");
    let address2GUID = formAutofillStorage.addresses.add(PTU.Addresses.TimBL2);
    await onChanged;

    await spawnPaymentDialogTask(frame, async function checkAdd({
      address1GUID,
      address2GUID,
      card1GUID,
    }) {
      info("checkAdd");
      let contentWin = Cu.waiveXrays(content);
      let {
        savedAddresses,
        savedBasicCards,
      } = contentWin.document.querySelector("payment-dialog").requestStore.getState();

      let addressGUIDs = Object.keys(savedAddresses);
      is(addressGUIDs.length, 2, "Now two savedAddresses");
      is(addressGUIDs[0], address1GUID, "Check first address GUID");
      is(savedAddresses[address1GUID].guid, address1GUID, "Check address 1 guid matches key");
      is(addressGUIDs[1], address2GUID, "Check second address GUID");
      is(savedAddresses[address2GUID].guid, address2GUID, "Check address 2 guid matches key");

      is(Object.keys(savedBasicCards).length, 1, "Still one savedBasicCards");
      is(savedBasicCards[card1GUID].guid, card1GUID, "Check card guid matches key");
      is(savedBasicCards[card1GUID].methodName, "basic-card",
         "Check card has a methodName of basic-card");
    }, {
      address1GUID,
      address2GUID,
      card1GUID,
    });

    onChanged = TestUtils.topicObserved("formautofill-storage-changed",
                                        (subject, data) => data == "update");
    info("updating the credit expiration");
    formAutofillStorage.creditCards.update(card1GUID, {
      "cc-exp-month": 6,
      "cc-exp-year": 2029,
    }, true);
    await onChanged;

    await spawnPaymentDialogTask(frame, async function checkUpdate({
      address1GUID,
      address2GUID,
      card1GUID,
    }) {
      info("checkUpdate");
      let contentWin = Cu.waiveXrays(content);
      let {
        savedAddresses,
        savedBasicCards,
      } = contentWin.document.querySelector("payment-dialog").requestStore.getState();

      let addressGUIDs = Object.keys(savedAddresses);
      is(addressGUIDs.length, 2, "Still two savedAddresses");
      is(addressGUIDs[0], address1GUID, "Check first address GUID");
      is(savedAddresses[address1GUID].guid, address1GUID, "Check address 1 guid matches key");
      is(addressGUIDs[1], address2GUID, "Check second address GUID");
      is(savedAddresses[address2GUID].guid, address2GUID, "Check address 2 guid matches key");

      is(Object.keys(savedBasicCards).length, 1, "Still one savedBasicCards");
      is(savedBasicCards[card1GUID].guid, card1GUID, "Check card guid matches key");
      is(savedBasicCards[card1GUID]["cc-exp-month"], 6, "Check expiry month");
      is(savedBasicCards[card1GUID]["cc-exp-year"], 2029, "Check expiry year");
      is(savedBasicCards[card1GUID].methodName, "basic-card",
         "Check card has a methodName of basic-card");
    }, {
      address1GUID,
      address2GUID,
      card1GUID,
    });

    onChanged = TestUtils.topicObserved("formautofill-storage-changed",
                                        (subject, data) => data == "remove");
    info("removing the first address");
    formAutofillStorage.addresses.remove(address1GUID);
    await onChanged;

    await spawnPaymentDialogTask(frame, async function checkRemove({
      address2GUID,
      card1GUID,
    }) {
      info("checkRemove");
      let contentWin = Cu.waiveXrays(content);
      let {
        savedAddresses,
        savedBasicCards,
      } = contentWin.document.querySelector("payment-dialog").requestStore.getState();

      is(Object.keys(savedAddresses).length, 1, "Now one savedAddresses");
      is(savedAddresses[address2GUID].name, "Timothy Johann Berners-Lee", "Check full name");
      is(savedAddresses[address2GUID].guid, address2GUID, "Check address guid matches key");

      is(Object.keys(savedBasicCards).length, 1, "Still one savedBasicCards");
      is(savedBasicCards[card1GUID]["cc-number"], "************1111", "Check cc-number");
      is(savedBasicCards[card1GUID].guid, card1GUID, "Check card guid matches key");
    }, {
      address2GUID,
      card1GUID,
    });

    spawnPaymentDialogTask(frame, PTU.DialogContentTasks.manuallyClickCancel);

    await BrowserTestUtils.waitForCondition(() => win.closed, "dialog should be closed");
  });
});
