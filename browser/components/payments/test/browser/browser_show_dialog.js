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
  formAutofillStorage.creditCards.update(card1GUID, {
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

    info("clicking pay");
    spawnPaymentDialogTask(frame, PTU.DialogContentTasks.completePayment);

    // Add a handler to complete the payment above.
    info("acknowledging the completion from the merchant page");
    let result = await ContentTask.spawn(browser, {}, PTU.ContentTasks.addCompletionHandler);

    is(result.response.shippingOption, "1", "Check shipping option");

    await BrowserTestUtils.waitForCondition(() => win.closed, "dialog should be closed");
  });
});

add_task(async function test_show_closeReject_dialog() {
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
    await ContentTask.spawn(browser, null, PTU.ContentTasks.catchShowPromiseRejection);

    info("Closing the dialog to reject the payment request");
    BrowserTestUtils.closeWindow(win);
    await BrowserTestUtils.waitForCondition(() => win.closed, "dialog should be closed");

    let result = await ContentTask.spawn(browser, null, async () => content.rqResult);
    ok(result.showException, "Expected promise rejection from the rq.show() promise");
    ok(!result.response,
       "rq.show() shouldn't resolve to a response");
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
