"use strict";

add_task(addSampleAddressesAndBasicCard);

add_task(async function test_show_error_on_addresschange() {
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: BLANK_PAGE_URL,
  }, async browser => {
    let {win, frame} =
      await setupPaymentDialog(browser, {
        methodData: [PTU.MethodData.basicCard],
        details: PTU.Details.twoShippingOptions,
        options: PTU.Options.requestShippingOption,
        merchantTaskFn: PTU.ContentTasks.createAndShowRequest,
      }
    );

    info("setting up the event handler for shippingoptionchange");
    let EXPECTED_ERROR_TEXT = "Cannot ship with option 1 on days that end with Y";
    await ContentTask.spawn(browser, {
      eventName: "shippingoptionchange",
      details: {
        error: EXPECTED_ERROR_TEXT,
        shippingOptions: [],
        total: {
          label: "Grand total is!!!!!: ",
          amount: {
            value: "12",
            currency: "USD",
          },
        },
      },
    }, PTU.ContentTasks.updateWith);
    await spawnPaymentDialogTask(frame,
                                 PTU.DialogContentTasks.selectShippingOptionById,
                                 "1");
    info("awaiting the shippingoptionchange event");
    await ContentTask.spawn(browser, {
      eventName: "shippingoptionchange",
    }, PTU.ContentTasks.awaitPaymentRequestEventPromise);

    let errorText = await spawnPaymentDialogTask(frame,
                                                 PTU.DialogContentTasks.getElementTextContent,
                                                 "#error-text");
    is(errorText, EXPECTED_ERROR_TEXT, "Error text should be present on dialog");
    let errorsVisible =
      await spawnPaymentDialogTask(frame, PTU.DialogContentTasks.isElementVisible, "#error-text");
    ok(errorsVisible, "Error text should be visible");

    info("setting up the event handler for shippingaddresschange");
    await ContentTask.spawn(browser, {
      eventName: "shippingaddresschange",
      details: {
        error: "",
        shippingOptions: PTU.Details.twoShippingOptions.shippingOptions,
        total: {
          label: "Grand total is now: ",
          amount: {
            value: "24",
            currency: "USD",
          },
        },
      },
    }, PTU.ContentTasks.updateWith);
    await spawnPaymentDialogTask(frame,
                                 PTU.DialogContentTasks.selectShippingAddressByCountry,
                                 "DE");
    info("awaiting the shippingaddresschange event");
    await ContentTask.spawn(browser, {
      eventName: "shippingaddresschange",
    }, PTU.ContentTasks.awaitPaymentRequestEventPromise);
    errorText = await spawnPaymentDialogTask(frame,
                                             PTU.DialogContentTasks.getElementTextContent,
                                             "#error-text");
    is(errorText, "", "Error text should not be present on dialog");
    errorsVisible =
      await spawnPaymentDialogTask(frame, PTU.DialogContentTasks.isElementVisible, "#error-text");
    ok(!errorsVisible, "Error text should not be visible");

    info("clicking cancel");
    spawnPaymentDialogTask(frame, PTU.DialogContentTasks.manuallyClickCancel);

    await BrowserTestUtils.waitForCondition(() => win.closed, "dialog should be closed");
  });
});
