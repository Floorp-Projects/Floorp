/* eslint-disable no-shadow */

"use strict";

add_task(addSampleAddressesAndBasicCard);

add_task(async function test_show_error_on_addresschange() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: BLANK_PAGE_URL,
    },
    async browser => {
      let { win, frame } = await setupPaymentDialog(browser, {
        methodData: [PTU.MethodData.basicCard],
        details: Object.assign(
          {},
          PTU.Details.twoShippingOptions,
          PTU.Details.total2USD
        ),
        options: PTU.Options.requestShippingOption,
        merchantTaskFn: PTU.ContentTasks.createAndShowRequest,
      });

      info("setting up the event handler for shippingoptionchange");
      await ContentTask.spawn(
        browser,
        {
          eventName: "shippingoptionchange",
          details: Object.assign(
            {},
            PTU.Details.genericShippingError,
            PTU.Details.noShippingOptions,
            PTU.Details.total2USD
          ),
        },
        PTU.ContentTasks.updateWith
      );

      await spawnPaymentDialogTask(
        frame,
        PTU.DialogContentTasks.selectShippingOptionById,
        "1"
      );

      info("awaiting the shippingoptionchange event");
      await ContentTask.spawn(
        browser,
        {
          eventName: "shippingoptionchange",
        },
        PTU.ContentTasks.awaitPaymentEventPromise
      );

      await spawnPaymentDialogTask(
        frame,
        expectedText => {
          let errorText = content.document.querySelector("header .page-error");
          is(
            errorText.textContent,
            expectedText,
            "Error text should be on dialog"
          );
          ok(content.isVisible(errorText), "Error text should be visible");
        },
        PTU.Details.genericShippingError.error
      );

      info("setting up the event handler for shippingaddresschange");
      await ContentTask.spawn(
        browser,
        {
          eventName: "shippingaddresschange",
          details: Object.assign(
            {},
            PTU.Details.noError,
            PTU.Details.twoShippingOptions,
            PTU.Details.total2USD
          ),
        },
        PTU.ContentTasks.updateWith
      );

      await selectPaymentDialogShippingAddressByCountry(frame, "DE");

      info("awaiting the shippingaddresschange event");
      await ContentTask.spawn(
        browser,
        {
          eventName: "shippingaddresschange",
        },
        PTU.ContentTasks.awaitPaymentEventPromise
      );

      await spawnPaymentDialogTask(frame, () => {
        let errorText = content.document.querySelector("header .page-error");
        is(errorText.textContent, "", "Error text should not be on dialog");
        ok(content.isHidden(errorText), "Error text should not be visible");
      });

      info("clicking cancel");
      spawnPaymentDialogTask(frame, PTU.DialogContentTasks.manuallyClickCancel);

      await BrowserTestUtils.waitForCondition(
        () => win.closed,
        "dialog should be closed"
      );
    }
  );
});

add_task(async function test_show_field_specific_error_on_addresschange() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: BLANK_PAGE_URL,
    },
    async browser => {
      let { win, frame } = await setupPaymentDialog(browser, {
        methodData: [PTU.MethodData.basicCard],
        details: Object.assign(
          {},
          PTU.Details.twoShippingOptions,
          PTU.Details.total2USD
        ),
        options: PTU.Options.requestShippingOption,
        merchantTaskFn: PTU.ContentTasks.createAndShowRequest,
      });

      info("setting up the event handler for shippingaddresschange");
      await ContentTask.spawn(
        browser,
        {
          eventName: "shippingaddresschange",
          details: Object.assign(
            {},
            PTU.Details.fieldSpecificErrors,
            PTU.Details.noShippingOptions,
            PTU.Details.total2USD
          ),
        },
        PTU.ContentTasks.updateWith
      );

      spawnPaymentDialogTask(
        frame,
        PTU.DialogContentTasks.selectShippingAddressByCountry,
        "DE"
      );

      info("awaiting the shippingaddresschange event");
      await ContentTask.spawn(
        browser,
        {
          eventName: "shippingaddresschange",
        },
        PTU.ContentTasks.awaitPaymentEventPromise
      );

      await spawnPaymentDialogTask(frame, async () => {
        let { PaymentTestUtils: PTU } = ChromeUtils.import(
          "resource://testing-common/PaymentTestUtils.jsm"
        );

        await PTU.DialogContentUtils.waitForState(
          content,
          state => {
            return Object.keys(
              state.request.paymentDetails.shippingAddressErrors
            ).length;
          },
          "Check that there are shippingAddressErrors"
        );

        is(
          content.document.querySelector("header .page-error").textContent,
          PTU.Details.fieldSpecificErrors.error,
          "Error text should be present on dialog"
        );

        info("click the Edit link");
        content.document
          .querySelector("address-picker.shipping-related .edit-link")
          .click();

        await PTU.DialogContentUtils.waitForState(
          content,
          state => {
            return (
              state.page.id == "shipping-address-page" &&
              state["shipping-address-page"].guid
            );
          },
          "Check edit page state"
        );

        // check errors and make corrections
        let addressForm = content.document.querySelector(
          "#shipping-address-page"
        );
        let { shippingAddressErrors } = PTU.Details.fieldSpecificErrors;
        is(
          addressForm.querySelectorAll(".error-text:not(:empty)").length,
          Object.keys(shippingAddressErrors).length - 1,
          "Each error should be presented, but only one of region and regionCode are displayed"
        );
        let errorFieldMap = Cu.waiveXrays(addressForm)._errorFieldMap;
        for (let [errorName, errorValue] of Object.entries(
          shippingAddressErrors
        )) {
          if (errorName == "region" || errorName == "regionCode") {
            errorValue = shippingAddressErrors.regionCode;
          }
          let fieldSelector = errorFieldMap[errorName];
          let containerSelector = fieldSelector + "-container";
          let container = addressForm.querySelector(containerSelector);
          try {
            is(
              container.querySelector(".error-text").textContent,
              errorValue,
              "Field specific error should be associated with " + errorName
            );
          } catch (ex) {
            ok(
              false,
              `no container for ${errorName}. selector= ${containerSelector}`
            );
          }
          try {
            let field = addressForm.querySelector(fieldSelector);
            let oldValue = field.value;
            if (field.localName == "select") {
              // Flip between empty and the selected entry so country fields won't change.
              content.fillField(field, "");
              content.fillField(field, oldValue);
            } else {
              content.fillField(
                field,
                field.value
                  .split("")
                  .reverse()
                  .join("")
              );
            }
          } catch (ex) {
            ok(
              false,
              `no field found for ${errorName}. selector= ${fieldSelector}`
            );
          }
        }
      });

      info(
        "setting up the event handler for a 2nd shippingaddresschange with a different error"
      );
      await ContentTask.spawn(
        browser,
        {
          eventName: "shippingaddresschange",
          details: Object.assign(
            {},
            {
              shippingAddressErrors: {
                phone: "Invalid phone number",
              },
            },
            PTU.Details.noShippingOptions,
            PTU.Details.total2USD
          ),
        },
        PTU.ContentTasks.updateWith
      );

      await spawnPaymentDialogTask(
        frame,
        PTU.DialogContentTasks.clickPrimaryButton
      );

      await spawnPaymentDialogTask(frame, async function checkForNewErrors() {
        let { PaymentTestUtils: PTU } = ChromeUtils.import(
          "resource://testing-common/PaymentTestUtils.jsm"
        );

        await PTU.DialogContentUtils.waitForState(
          content,
          state => {
            return (
              state.page.id == "payment-summary" &&
              state.request.paymentDetails.shippingAddressErrors.phone ==
                "Invalid phone number"
            );
          },
          "Check the new error is in state"
        );

        ok(
          content.document
            .querySelector("#payment-summary")
            .innerText.includes("Invalid phone number"),
          "Check error visibility on summary page"
        );
        ok(
          content.document.getElementById("pay").disabled,
          "Pay button should be disabled until the field error is addressed"
        );
      });

      await navigateToAddShippingAddressPage(frame, {
        addLinkSelector:
          'address-picker[selected-state-key="selectedShippingAddress"] .edit-link',
      });

      await spawnPaymentDialogTask(
        frame,
        async function checkForNewErrorOnEdit() {
          let addressForm = content.document.querySelector(
            "#shipping-address-page"
          );
          is(
            addressForm.querySelectorAll(".error-text:not(:empty)").length,
            1,
            "Check one error shown"
          );
        }
      );

      await fillInShippingAddressForm(frame, {
        tel: PTU.Addresses.TimBL2.tel,
      });

      info("setup updateWith to clear errors");
      await ContentTask.spawn(
        browser,
        {
          eventName: "shippingaddresschange",
          details: Object.assign(
            {},
            PTU.Details.twoShippingOptions,
            PTU.Details.total2USD
          ),
        },
        PTU.ContentTasks.updateWith
      );

      await spawnPaymentDialogTask(
        frame,
        PTU.DialogContentTasks.clickPrimaryButton
      );

      await spawnPaymentDialogTask(frame, async function fixLastError() {
        let { PaymentTestUtils: PTU } = ChromeUtils.import(
          "resource://testing-common/PaymentTestUtils.jsm"
        );

        await PTU.DialogContentUtils.waitForState(
          content,
          state => {
            return state.page.id == "payment-summary";
          },
          "Check we're back on summary view"
        );

        await PTU.DialogContentUtils.waitForState(
          content,
          state => {
            return !Object.keys(
              state.request.paymentDetails.shippingAddressErrors
            ).length;
          },
          "Check that there are no more shippingAddressErrors"
        );

        is(
          content.document.querySelector("header .page-error").textContent,
          "",
          "Error text should not be present on dialog"
        );

        info("click the Edit link again");
        content.document.querySelector("address-picker .edit-link").click();

        await PTU.DialogContentUtils.waitForState(
          content,
          state => {
            return (
              state.page.id == "shipping-address-page" &&
              state["shipping-address-page"].guid
            );
          },
          "Check edit page state"
        );

        let addressForm = content.document.querySelector(
          "#shipping-address-page"
        );
        // check no errors present
        let errorTextSpans = addressForm.querySelectorAll(
          ".error-text:not(:empty)"
        );
        for (let errorTextSpan of errorTextSpans) {
          is(
            errorTextSpan.textContent,
            "",
            "No errors should be present on the field"
          );
        }

        info("click the Back button");
        addressForm.querySelector(".back-button").click();

        await PTU.DialogContentUtils.waitForState(
          content,
          state => {
            return state.page.id == "payment-summary";
          },
          "Check we're back on summary view"
        );
      });

      info("clicking cancel");
      spawnPaymentDialogTask(frame, PTU.DialogContentTasks.manuallyClickCancel);

      await BrowserTestUtils.waitForCondition(
        () => win.closed,
        "dialog should be closed"
      );
    }
  );
});
