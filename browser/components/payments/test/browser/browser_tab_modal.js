"use strict";

const methodData = [PTU.MethodData.basicCard];
const details = Object.assign(
  {},
  PTU.Details.twoShippingOptions,
  PTU.Details.total2USD
);

async function checkTabModal(browser, win, msg) {
  info(`checkTabModal: ${msg}`);
  let doc = browser.ownerDocument;
  await TestUtils.waitForCondition(() => {
    return !doc.querySelector(".paymentDialogContainer").hidden;
  }, "Waiting for container to be visible after the dialog's ready");
  is(
    doc.querySelectorAll(".paymentDialogContainer").length,
    1,
    "Only 1 paymentDialogContainer"
  );
  ok(!EventUtils.isHidden(win.frameElement), "Frame should be visible");

  let { bottom: toolboxBottom } = doc
    .getElementById("navigator-toolbox")
    .getBoundingClientRect();

  let { x, y } = win.frameElement.getBoundingClientRect();
  ok(y > 0, "Frame should have y > 0");
  // Inset by 10px since the corner point doesn't return the frame due to the
  // border-radius.
  is(
    doc.elementFromPoint(x + 10, y + 10),
    win.frameElement,
    "Check .paymentDialogContainerFrame is visible"
  );

  info("Click to the left of the dialog over the content area");
  isnot(
    doc.elementFromPoint(x - 10, y + 50),
    browser,
    "Check clicks on the merchant content area don't go to the browser"
  );
  is(
    doc.elementFromPoint(x - 10, y + 50),
    doc.querySelector(".paymentDialogBackground"),
    "Check clicks on the merchant content area go to the payment dialog background"
  );

  ok(
    y < toolboxBottom - 2,
    "Dialog should overlap the toolbox by at least 2px"
  );

  ok(
    browser.hasAttribute("tabmodalPromptShowing"),
    "Check browser has @tabmodalPromptShowing"
  );

  return {
    x,
    y,
  };
}

add_task(async function test_tab_modal() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: BLANK_PAGE_URL,
    },
    async browser => {
      let { win, frame } = await setupPaymentDialog(browser, {
        methodData,
        details,
        merchantTaskFn: PTU.ContentTasks.createAndShowRequest,
      });

      let { x, y } = await checkTabModal(browser, win, "initial dialog");

      await BrowserTestUtils.withNewTab(
        {
          gBrowser,
          url: BLANK_PAGE_URL,
        },
        async newBrowser => {
          let { x: x2, y: y2 } = win.frameElement.getBoundingClientRect();
          is(x2, x, "Check x-coordinate is the same");
          is(y2, y, "Check y-coordinate is the same");
          isnot(
            document.elementFromPoint(x + 10, y + 10),
            win.frameElement,
            "Check .paymentDialogContainerFrame is hidden"
          );
          ok(
            !newBrowser.hasAttribute("tabmodalPromptShowing"),
            "Check second browser doesn't have @tabmodalPromptShowing"
          );
        }
      );

      let { x: x3, y: y3 } = await checkTabModal(
        browser,
        win,
        "after tab switch back"
      );
      is(x3, x, "Check x-coordinate is the same again");
      is(y3, y, "Check y-coordinate is the same again");

      spawnPaymentDialogTask(frame, PTU.DialogContentTasks.manuallyClickCancel);
      await BrowserTestUtils.waitForCondition(
        () => win.closed,
        "dialog should be closed"
      );

      await BrowserTestUtils.waitForCondition(
        () => !browser.hasAttribute("tabmodalPromptShowing"),
        "Check @tabmodalPromptShowing was removed"
      );
    }
  );
});

add_task(async function test_detachToNewWindow() {
  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: BLANK_PAGE_URL,
  });
  let browser = tab.linkedBrowser;

  let { frame, requestId } = await setupPaymentDialog(browser, {
    methodData,
    details,
    merchantTaskFn: PTU.ContentTasks.createAndShowRequest,
  });

  is(
    Object.values(frame.paymentDialogWrapper.temporaryStore.addresses.getAll())
      .length,
    0,
    "Check initial temp. address store"
  );
  is(
    Object.values(
      frame.paymentDialogWrapper.temporaryStore.creditCards.getAll()
    ).length,
    0,
    "Check initial temp. card store"
  );

  info("Create some temp. records so we can later check if they are preserved");
  let address1 = { ...PTU.Addresses.Temp };
  let card1 = { ...PTU.BasicCards.JaneMasterCard, ...{ "cc-csc": "123" } };

  await fillInBillingAddressForm(frame, address1, {
    setPersistCheckedValue: false,
  });

  await spawnPaymentDialogTask(
    frame,
    PTU.DialogContentTasks.clickPrimaryButton
  );

  await spawnPaymentDialogTask(frame, async function waitForPageChange() {
    let { PaymentTestUtils: PTU } = ChromeUtils.import(
      "resource://testing-common/PaymentTestUtils.jsm"
    );

    await PTU.DialogContentUtils.waitForState(
      content,
      state => {
        return state.page.id == "basic-card-page";
      },
      "Wait for basic-card-page"
    );
  });

  await fillInCardForm(frame, card1, {
    checkboxSelector: "basic-card-form .persist-checkbox",
    setPersistCheckedValue: false,
  });

  await spawnPaymentDialogTask(
    frame,
    PTU.DialogContentTasks.clickPrimaryButton
  );

  let { temporaryStore } = frame.paymentDialogWrapper;
  TestUtils.waitForCondition(() => {
    return Object.values(temporaryStore.addresses.getAll()).length == 1;
  }, "Check address store");
  TestUtils.waitForCondition(() => {
    return Object.values(temporaryStore.creditCards.getAll()).length == 1;
  }, "Check card store");

  let windowLoadedPromise = BrowserTestUtils.waitForNewWindow();
  let newWin = gBrowser.replaceTabWithWindow(tab);
  await windowLoadedPromise;

  info("tab was detached");
  let newBrowser = newWin.gBrowser.selectedBrowser;
  ok(newBrowser, "Found new <browser>");

  let widget = await TestUtils.waitForCondition(async () =>
    getPaymentWidget(requestId)
  );
  await checkTabModal(newBrowser, widget, "after detach");

  let state = await spawnPaymentDialogTask(
    widget.frameElement,
    async function checkAfterDetach() {
      let { PaymentTestUtils: PTU } = ChromeUtils.import(
        "resource://testing-common/PaymentTestUtils.jsm"
      );

      return PTU.DialogContentUtils.getCurrentState(content);
    }
  );

  is(
    Object.values(state.tempAddresses).length,
    1,
    "Check still 1 temp. address in state"
  );
  is(
    Object.values(state.tempBasicCards).length,
    1,
    "Check still 1 temp. basic card in state"
  );

  temporaryStore = widget.frameElement.paymentDialogWrapper.temporaryStore;
  is(
    Object.values(temporaryStore.addresses.getAll()).length,
    1,
    "Check address store in wrapper"
  );
  is(
    Object.values(temporaryStore.creditCards.getAll()).length,
    1,
    "Check card store in wrapper"
  );

  info(
    "Check that the message manager and formautofill-storage-changed observer are connected"
  );
  is(Object.values(state.savedAddresses).length, 0, "Check 0 saved addresses");
  await addAddressRecord(PTU.Addresses.TimBL2);
  await spawnPaymentDialogTask(
    widget.frameElement,
    async function waitForSavedAddress() {
      let { PaymentTestUtils: PTU } = ChromeUtils.import(
        "resource://testing-common/PaymentTestUtils.jsm"
      );

      await PTU.DialogContentUtils.waitForState(
        content,
        function checkSavedAddresses(s) {
          return Object.values(s.savedAddresses).length == 1;
        },
        "Check 1 saved address in state"
      );
    }
  );

  info(
    "re-attach the tab back in the original window to test the event listeners were added"
  );

  let tab3 = gBrowser.adoptTab(newWin.gBrowser.selectedTab, 1, true);
  widget = await TestUtils.waitForCondition(async () =>
    getPaymentWidget(requestId)
  );
  is(
    widget.frameElement.ownerGlobal,
    window,
    "Check widget is back in first window"
  );
  await checkTabModal(tab3.linkedBrowser, widget, "after re-attaching");

  temporaryStore = widget.frameElement.paymentDialogWrapper.temporaryStore;
  is(
    Object.values(temporaryStore.addresses.getAll()).length,
    1,
    "Check temp addresses in wrapper"
  );
  is(
    Object.values(temporaryStore.creditCards.getAll()).length,
    1,
    "Check temp cards in wrapper"
  );

  spawnPaymentDialogTask(
    widget.frameElement,
    PTU.DialogContentTasks.manuallyClickCancel
  );
  await BrowserTestUtils.waitForCondition(
    () => widget.closed,
    "dialog should be closed"
  );
  await BrowserTestUtils.removeTab(tab3);
});
