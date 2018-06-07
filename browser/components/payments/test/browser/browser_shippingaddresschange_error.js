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
        details: Object.assign({},
                               PTU.Details.twoShippingOptions,
                               PTU.Details.total2USD),
        options: PTU.Options.requestShippingOption,
        merchantTaskFn: PTU.ContentTasks.createAndShowRequest,
      }
    );

    info("setting up the event handler for shippingoptionchange");
    await ContentTask.spawn(browser, {
      eventName: "shippingoptionchange",
      details: Object.assign({},
                             PTU.Details.genericShippingError,
                             PTU.Details.noShippingOptions,
                             PTU.Details.total2USD),
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
    }, PTU.Details.genericShippingError.error);

    info("setting up the event handler for shippingaddresschange");
    await ContentTask.spawn(browser, {
      eventName: "shippingaddresschange",
      details: Object.assign({},
                             PTU.Details.noError,
                             PTU.Details.twoShippingOptions,
                             PTU.Details.total2USD),
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

add_task(async function test_show_field_specific_error_on_addresschange() {
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: BLANK_PAGE_URL,
  }, async browser => {
    let {win, frame} =
      await setupPaymentDialog(browser, {
        methodData: [PTU.MethodData.basicCard],
        details: Object.assign({},
                               PTU.Details.twoShippingOptions,
                               PTU.Details.total2USD),
        options: PTU.Options.requestShippingOption,
        merchantTaskFn: PTU.ContentTasks.createAndShowRequest,
      }
    );

    info("setting up the event handler for shippingaddresschange");
    await ContentTask.spawn(browser, {
      eventName: "shippingaddresschange",
      details: Object.assign({},
                             PTU.Details.fieldSpecificErrors,
                             PTU.Details.noShippingOptions,
                             PTU.Details.total2USD),
    }, PTU.ContentTasks.updateWith);

    spawnPaymentDialogTask(frame, PTU.DialogContentTasks.selectShippingAddressByCountry, "DE");

    info("awaiting the shippingaddresschange event");
    await ContentTask.spawn(browser, {
      eventName: "shippingaddresschange",
    }, PTU.ContentTasks.awaitPaymentRequestEventPromise);

    await spawnPaymentDialogTask(frame, async () => {
      let {
        PaymentTestUtils: PTU,
      } = ChromeUtils.import("resource://testing-common/PaymentTestUtils.jsm", {});

      await PTU.DialogContentUtils.waitForState(content, (state) => {
        return Object.keys(state.request.paymentDetails.shippingAddressErrors).length;
      }, "Check that there are shippingAddressErrors");

      is(content.document.querySelector("#error-text").textContent,
         PTU.Details.fieldSpecificErrors.error,
         "Error text should be present on dialog");

      info("click the Edit link");
      content.document.querySelector("address-picker .edit-link").click();

      await PTU.DialogContentUtils.waitForState(content, (state) => {
        return state.page.id == "address-page" && state["address-page"].guid;
      }, "Check edit page state");

      // check errors and make corrections
      let {shippingAddressErrors} = PTU.Details.fieldSpecificErrors;
      is(content.document.querySelectorAll("address-form .error-text:not(:empty)").length,
         Object.keys(shippingAddressErrors).length,
         "Each error should be presented");
      let errorFieldMap =
        Cu.waiveXrays(content.document.querySelector("address-form"))._errorFieldMap;
      for (let [errorName, errorValue] of Object.entries(shippingAddressErrors)) {
        let field = content.document.querySelector(errorFieldMap[errorName] + "-container");
        try {
          is(field.querySelector(".error-text").textContent, errorValue,
             "Field specific error should be associated with " + errorName);
        } catch (ex) {
          ok(false, `no field found for ${errorName}. selector= ${errorFieldMap[errorName]}`);
        }
      }
    });

    info("setup updateWith to clear errors");
    await ContentTask.spawn(browser, {
      eventName: "shippingaddresschange",
      details: Object.assign({},
                             PTU.Details.twoShippingOptions,
                             PTU.Details.total2USD),
    }, PTU.ContentTasks.updateWith);

    await spawnPaymentDialogTask(frame, async () => {
      let {
        PaymentTestUtils: PTU,
      } = ChromeUtils.import("resource://testing-common/PaymentTestUtils.jsm", {});

      info("saving corrections");
      content.document.querySelector("address-form .save-button").click();

      await PTU.DialogContentUtils.waitForState(content, (state) => {
        return state.page.id == "payment-summary";
      }, "Check we're back on summary view");

      await PTU.DialogContentUtils.waitForState(content, (state) => {
        return !Object.keys(state.request.paymentDetails.shippingAddressErrors).length;
      }, "Check that there are no more shippingAddressErrors");

      is(content.document.querySelector("#error-text").textContent,
         "", "Error text should not be present on dialog");

      info("click the Edit link again");
      content.document.querySelector("address-picker .edit-link").click();

      await PTU.DialogContentUtils.waitForState(content, (state) => {
        return state.page.id == "address-page" && state["address-page"].guid;
      }, "Check edit page state");

      // check no errors present
      let errorTextSpans =
        content.document.querySelectorAll("address-form .error-text:not(:empty)");
      for (let errorTextSpan of errorTextSpans) {
        is(errorTextSpan.textContent, "", "No errors should be present on the field");
      }

      info("click the Back button");
      content.document.querySelector("address-form .back-button").click();

      await PTU.DialogContentUtils.waitForState(content, (state) => {
        return state.page.id == "payment-summary";
      }, "Check we're back on summary view");
    });

    info("clicking cancel");
    spawnPaymentDialogTask(frame, PTU.DialogContentTasks.manuallyClickCancel);

    await BrowserTestUtils.waitForCondition(() => win.closed, "dialog should be closed");
  });
});
