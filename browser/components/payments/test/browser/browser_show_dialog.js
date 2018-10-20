"use strict";

const methodData = [PTU.MethodData.basicCard];
const details = Object.assign({}, PTU.Details.twoShippingOptions, PTU.Details.total2USD);

add_task(async function test_show_abort_dialog() {
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: BLANK_PAGE_URL,
  }, async browser => {
    let {win} =
      await setupPaymentDialog(browser, {
        methodData,
        details,
        merchantTaskFn: PTU.ContentTasks.createAndShowRequest,
      }
    );

    // abort the payment request
    ContentTask.spawn(browser, null, async () => content.rq.abort());
    await BrowserTestUtils.waitForCondition(() => win.closed, "dialog should be closed");
  });
});

add_task(async function test_show_manualAbort_dialog() {
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

    spawnPaymentDialogTask(frame, PTU.DialogContentTasks.manuallyClickCancel);
    await BrowserTestUtils.waitForCondition(() => win.closed, "dialog should be closed");
  });
});

add_task(async function test_show_completePayment() {
  let {address1GUID, card1GUID} = await addSampleAddressesAndBasicCard();

  let onChanged = TestUtils.topicObserved("formautofill-storage-changed",
                                          (subject, data) => data == "update");
  info("associating the card with the billing address");
  await formAutofillStorage.creditCards.update(card1GUID, {
    billingAddressGUID: address1GUID,
  }, true);
  await onChanged;

  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: BLANK_PAGE_URL,
  }, async browser => {
    let {win, frame} =
      await setupPaymentDialog(browser, {
        methodData,
        details,
        options: PTU.Options.requestShippingOption,
        merchantTaskFn: PTU.ContentTasks.createAndShowRequest,
      }
    );

    info("select the shipping address");
    await selectPaymentDialogShippingAddressByCountry(frame, "US");

    info("entering CSC");
    await spawnPaymentDialogTask(frame, PTU.DialogContentTasks.setSecurityCode, {
      securityCode: "999",
    });
    info("clicking pay");
    spawnPaymentDialogTask(frame, PTU.DialogContentTasks.completePayment);

    // Add a handler to complete the payment above.
    info("acknowledging the completion from the merchant page");
    let result = await ContentTask.spawn(browser, {}, PTU.ContentTasks.addCompletionHandler);

    let {shippingAddress} = result.response;
    checkPaymentAddressMatchesStorageAddress(shippingAddress, PTU.Addresses.TimBL, "Shipping");

    is(result.response.methodName, "basic-card", "Check methodName");
    let {methodDetails} = result;
    checkPaymentMethodDetailsMatchesCard(methodDetails, PTU.BasicCards.JohnDoe, "Payment method");
    is(methodDetails.cardSecurityCode, "999", "Check cardSecurityCode");
    is(typeof methodDetails.methodName, "undefined", "Check methodName wasn't included");

    checkPaymentAddressMatchesStorageAddress(methodDetails.billingAddress, PTU.Addresses.TimBL,
                                             "Billing address");

    is(result.response.shippingOption, "2", "Check shipping option");

    await BrowserTestUtils.waitForCondition(() => win.closed, "dialog should be closed");
  });
});

add_task(async function test_show_completePayment2() {
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: BLANK_PAGE_URL,
  }, async browser => {
    let {win, frame} =
      await setupPaymentDialog(browser, {
        methodData,
        details,
        options: PTU.Options.requestShippingOption,
        merchantTaskFn: PTU.ContentTasks.createAndShowRequest,
      }
    );

    await ContentTask.spawn(browser, {
      eventName: "shippingoptionchange",
    }, PTU.ContentTasks.promisePaymentRequestEvent);

    info("changing shipping option to '1' from default selected option of '2'");
    await spawnPaymentDialogTask(frame, PTU.DialogContentTasks.selectShippingOptionById, "1");

    await ContentTask.spawn(browser, {
      eventName: "shippingoptionchange",
    }, PTU.ContentTasks.awaitPaymentRequestEventPromise);
    info("got shippingoptionchange event");

    info("select the shipping address");
    await selectPaymentDialogShippingAddressByCountry(frame, "US");

    info("entering CSC");
    await spawnPaymentDialogTask(frame, PTU.DialogContentTasks.setSecurityCode, {
      securityCode: "123",
    });

    info("clicking pay");
    spawnPaymentDialogTask(frame, PTU.DialogContentTasks.completePayment);

    // Add a handler to complete the payment above.
    info("acknowledging the completion from the merchant page");
    let result = await ContentTask.spawn(browser, {}, PTU.ContentTasks.addCompletionHandler);

    is(result.response.shippingOption, "1", "Check shipping option");

    await BrowserTestUtils.waitForCondition(() => win.closed, "dialog should be closed");
  });
});

add_task(async function test_localized() {
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

    await spawnPaymentDialogTask(frame, async function check_l10n() {
      await ContentTaskUtils.waitForCondition(() => {
        let telephoneLabel = content.document.querySelector("#tel-container > .label-text");
        return telephoneLabel && telephoneLabel.textContent.includes("Phone");
      }, "Check that the telephone number label is localized");

      await ContentTaskUtils.waitForCondition(() => {
        let ccNumberField = content.document.querySelector("#cc-number");
        if (!ccNumberField) {
          return false;
        }
        let ccNumberLabel = ccNumberField.parentElement.querySelector(".label-text");
        return ccNumberLabel.textContent.includes("Number");
      }, "Check that the cc-number label is localized");

      const L10N_ATTRIBUTE_SELECTOR = "[data-localization], [data-localization-region]";
      await ContentTaskUtils.waitForCondition(() => {
        return content.document.querySelectorAll(L10N_ATTRIBUTE_SELECTOR).length === 0;
      }, "Check that there are no unlocalized strings");
    });

    // abort the payment request
    ContentTask.spawn(browser, null, async () => content.rq.abort());
    await BrowserTestUtils.waitForCondition(() => win.closed, "dialog should be closed");
  });
});

add_task(async function test_supportedNetworks() {
  await setupFormAutofillStorage();
  await cleanupFormAutofillStorage();

  let address1GUID = await addAddressRecord(PTU.Addresses.TimBL);
  let visaCardGUID = await addCardRecord(Object.assign({}, PTU.BasicCards.JohnDoe, {
    billingAddressGUID: address1GUID,
  }));
  let masterCardGUID = await addCardRecord(Object.assign({}, PTU.BasicCards.JaneMasterCard, {
    billingAddressGUID: address1GUID,
  }));

  let cardMethod = {
    supportedMethods: "basic-card",
    data: {
      supportedNetworks: ["visa"],
    },
  };

  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: BLANK_PAGE_URL,
  }, async browser => {
    let {win, frame} =
      await setupPaymentDialog(browser, {
        methodData: [cardMethod],
        details,
        merchantTaskFn: PTU.ContentTasks.createAndShowRequest,
      }
    );

    info("entering CSC");
    await spawnPaymentDialogTask(frame, PTU.DialogContentTasks.setSecurityCode, {
      securityCode: "789",
    });

    await spawnPaymentDialogTask(frame, () => {
      let acceptedCards = content.document.querySelector("accepted-cards");
      ok(acceptedCards && !content.isHidden(acceptedCards),
         "accepted-cards element is present and visible");
      is(Cu.waiveXrays(acceptedCards).acceptedItems.length, 1,
         "accepted-cards element has 1 item");
    });

    info("select the mastercard using guid: " + masterCardGUID);
    await spawnPaymentDialogTask(frame,
                                 PTU.DialogContentTasks.selectPaymentOptionByGuid,
                                 masterCardGUID);

    info("spawn task to check pay button with mastercard selected");
    await spawnPaymentDialogTask(frame, async () => {
      ok(content.document.getElementById("pay").disabled, "pay button should be disabled");
    });

    info("select the visa using guid: " + visaCardGUID);
    await spawnPaymentDialogTask(frame,
                                 PTU.DialogContentTasks.selectPaymentOptionByGuid,
                                 visaCardGUID);

    info("spawn task to check pay button");
    await spawnPaymentDialogTask(frame, async () => {
      ok(!content.document.getElementById("pay").disabled, "pay button should not be disabled");
    });

    spawnPaymentDialogTask(frame, PTU.DialogContentTasks.manuallyClickCancel);
    await BrowserTestUtils.waitForCondition(() => win.closed, "dialog should be closed");
  });
});

add_task(async function test_tab_modal() {
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: BLANK_PAGE_URL,
  }, async browser => {
    let {win, frame} = await setupPaymentDialog(browser, {
      methodData,
      details,
      merchantTaskFn: PTU.ContentTasks.createAndShowRequest,
    });

    await TestUtils.waitForCondition(() => {
      return !document.querySelector(".paymentDialogContainer").hidden;
    }, "Waiting for container to be visible after the dialog's ready");

    ok(!EventUtils.isHidden(win.frameElement), "Frame should be visible");

    let {
      bottom: toolboxBottom,
    } = document.getElementById("navigator-toolbox").getBoundingClientRect();

    let {x, y} = win.frameElement.getBoundingClientRect();
    ok(y > 0, "Frame should have y > 0");
    // Inset by 10px since the corner point doesn't return the frame due to the
    // border-radius.
    is(document.elementFromPoint(x + 10, y + 10), win.frameElement,
       "Check .paymentDialogContainerFrame is visible");

    info("Click to the left of the dialog over the content area");
    isnot(document.elementFromPoint(x - 10, y + 50), browser,
          "Check clicks on the merchant content area don't go to the browser");
    is(document.elementFromPoint(x - 10, y + 50),
       document.querySelector(".paymentDialogBackground"),
       "Check clicks on the merchant content area go to the payment dialog background");

    ok(y < toolboxBottom - 2, "Dialog should overlap the toolbox by at least 2px");

    ok(browser.hasAttribute("tabmodalPromptShowing"), "Check browser has @tabmodalPromptShowing");

    await BrowserTestUtils.withNewTab({
      gBrowser,
      url: BLANK_PAGE_URL,
    }, async newBrowser => {
      let {
        x: x2,
        y: y2,
      } = win.frameElement.getBoundingClientRect();
      is(x2, x, "Check x-coordinate is the same");
      is(y2, y, "Check y-coordinate is the same");
      isnot(document.elementFromPoint(x + 10, y + 10), win.frameElement,
            "Check .paymentDialogContainerFrame is hidden");
      ok(!newBrowser.hasAttribute("tabmodalPromptShowing"),
         "Check second browser doesn't have @tabmodalPromptShowing");
    });

    let {
      x: x3,
      y: y3,
    } = win.frameElement.getBoundingClientRect();
    is(x3, x, "Check x-coordinate is the same again");
    is(y3, y, "Check y-coordinate is the same again");
    is(document.elementFromPoint(x + 10, y + 10), win.frameElement,
       "Check .paymentDialogContainerFrame is visible again");

    spawnPaymentDialogTask(frame, PTU.DialogContentTasks.manuallyClickCancel);
    await BrowserTestUtils.waitForCondition(() => win.closed, "dialog should be closed");

    await BrowserTestUtils.waitForCondition(() => !browser.hasAttribute("tabmodalPromptShowing"),
                                            "Check @tabmodalPromptShowing was removed");
  });
});
