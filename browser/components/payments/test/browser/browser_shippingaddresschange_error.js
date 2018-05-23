/* eslint-disable no-shadow */

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
    await ContentTask.spawn(browser, {
      eventName: "shippingoptionchange",
      details: PTU.UpdateWith.genericShippingError,
    }, PTU.ContentTasks.updateWith);

    await spawnPaymentDialogTask(frame, PTU.DialogContentTasks.selectShippingOptionById, "1");

    info("awaiting the shippingoptionchange event");
    await ContentTask.spawn(browser, {
      eventName: "shippingoptionchange",
    }, PTU.ContentTasks.awaitPaymentRequestEventPromise);

    await spawnPaymentDialogTask(frame, expectedText => {
      let errorText = content.document.querySelector("#error-text");
      is(errorText.textContent, expectedText, "Error text should be on dialog");
      ok(content.isVisible(errorText), "Error text should be visible");
    }, PTU.UpdateWith.genericShippingError.error);

    info("setting up the event handler for shippingaddresschange");
    await ContentTask.spawn(browser, {
      eventName: "shippingaddresschange",
      details: PTU.UpdateWith.twoShippingOptions,
    }, PTU.ContentTasks.updateWith);

    await selectPaymentDialogShippingAddressByCountry(frame, "DE");

    info("awaiting the shippingaddresschange event");
    await ContentTask.spawn(browser, {
      eventName: "shippingaddresschange",
    }, PTU.ContentTasks.awaitPaymentRequestEventPromise);

    await spawnPaymentDialogTask(frame, () => {
      let errorText = content.document.querySelector("#error-text");
      is(errorText.textContent, "", "Error text should not be on dialog");
      ok(content.isHidden(errorText), "Error text should not be visible");
    });

    info("clicking cancel");
    spawnPaymentDialogTask(frame, PTU.DialogContentTasks.manuallyClickCancel);

    await BrowserTestUtils.waitForCondition(() => win.closed, "dialog should be closed");
  });
});
