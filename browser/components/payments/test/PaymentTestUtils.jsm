"use strict";

var EXPORTED_SYMBOLS = ["PaymentTestUtils"];

var PaymentTestUtils = {
  /**
   * Common content tasks functions to be used with ContentTask.spawn.
   */
  ContentTasks: {
    /* eslint-env mozilla/frame-script */
    /**
     * Add a completion handler to the existing `showPromise` to call .complete().
     * @returns {Object} representing the PaymentResponse
     */
    addCompletionHandler: async () => {
      let response = await content.showPromise;
      response.complete();
      return {
        response: response.toJSON(),
        // XXX: Bug NNN: workaround for `details` not being included in `toJSON`.
        methodDetails: response.details,
      };
    },

    ensureNoPaymentRequestEvent: ({eventName}) => {
      content.rq.addEventListener(eventName, (event) => {
        ok(false, `Unexpected ${eventName}`);
      });
    },

    promisePaymentRequestEvent: ({eventName}) => {
      content[eventName + "Promise"] =
        new Promise(resolve => {
          content.rq.addEventListener(eventName, () => {
            resolve();
          });
        });
    },

    awaitPaymentRequestEventPromise: async ({eventName}) => {
      await content[eventName + "Promise"];
      return true;
    },

    updateWith: async ({eventName, details}) => {
      /* globals ok */
      if (details.error &&
          (!details.shippingOptions || details.shippingOptions.length)) {
        ok(false, "Need to clear the shipping options to show error text");
      }
      if (!details.total) {
        ok(false, "`total: { label, amount: { value, currency } }` is required for updateWith");
      }

      content[eventName + "Promise"] =
        new Promise(resolve => {
          content.rq.addEventListener(eventName, event => {
            event.updateWith(details);
            resolve();
          }, {once: true});
        });
    },

    /**
     * Create a new payment request and cache it as `rq`.
     *
     * @param {Object} args
     * @param {PaymentMethodData[]} methodData
     * @param {PaymentDetailsInit} details
     * @param {PaymentOptions} options
     */
    createRequest: ({methodData, details, options}) => {
      const rq = new content.PaymentRequest(methodData, details, options);
      content.rq = rq; // assign it so we can retrieve it later
    },

    /**
     * Create a new payment request cached as `rq` and then show it.
     *
     * @param {Object} args
     * @param {PaymentMethodData[]} methodData
     * @param {PaymentDetailsInit} details
     * @param {PaymentOptions} options
     */
    createAndShowRequest: ({methodData, details, options}) => {
      const rq = new content.PaymentRequest(methodData, details, options);
      content.rq = rq; // assign it so we can retrieve it later

      const handle = content.QueryInterface(Ci.nsIInterfaceRequestor)
                            .getInterface(Ci.nsIDOMWindowUtils)
                            .setHandlingUserInput(true);
      content.showPromise = rq.show();

      handle.destruct();
    },
  },

  DialogContentTasks: {
    isElementVisible: selector => {
      let element = content.document.querySelector(selector);
      return element.getBoundingClientRect().height > 0;
    },

    getElementTextContent: selector => {
      let doc = content.document;
      let element = doc.querySelector(selector);
      return element.textContent;
    },

    getShippingOptions: () => {
      let select = content.document.querySelector("shipping-option-picker > rich-select");
      let popupBox = Cu.waiveXrays(select).popupBox;
      let selectedOptionIndex = Array.from(popupBox.children)
                                     .findIndex(item => item.hasAttribute("selected"));
      let selectedOption = popupBox.children[selectedOptionIndex];
      let currencyAmount = selectedOption.querySelector("currency-amount");
      return {
        optionCount: popupBox.children.length,
        selectedOptionIndex,
        selectedOptionID: selectedOption.getAttribute("value"),
        selectedOptionLabel: selectedOption.getAttribute("label"),
        selectedOptionCurrency: currencyAmount.getAttribute("currency"),
        selectedOptionValue: currencyAmount.getAttribute("value"),
      };
    },

    getShippingAddresses: () => {
      let doc = content.document;
      let addressPicker =
        doc.querySelector("address-picker[selected-state-key='selectedShippingAddress']");
      let select = addressPicker.querySelector("rich-select");
      let popupBox = Cu.waiveXrays(select).popupBox;
      let options = Array.from(popupBox.children).map(option => {
        return {
          guid: option.guid,
          country: option.country,
          selected: option.selected,
        };
      });
      let selectedOptionIndex = options.findIndex(item => item.selected);
      return {
        selectedOptionIndex,
        options,
      };
    },

    selectShippingAddressByCountry: country => {
      let doc = content.document;
      let addressPicker =
        doc.querySelector("address-picker[selected-state-key='selectedShippingAddress']");
      let select = addressPicker.querySelector("rich-select");
      let option = select.querySelector(`[country="${country}"]`);
      select.click();
      option.click();
    },

    selectShippingOptionById: value => {
      let doc = content.document;
      let optionPicker =
        doc.querySelector("shipping-option-picker");
      let select = optionPicker.querySelector("rich-select");
      let option = select.querySelector(`[value="${value}"]`);
      select.click();
      option.click();
    },

    /**
     * Click the cancel button
     *
     * Don't await on this task since the cancel can close the dialog before
     * ContentTask can resolve the promise.
     *
     * @returns {undefined}
     */
    manuallyClickCancel: () => {
      content.document.getElementById("cancel").click();
    },

    /**
     * Do the minimum possible to complete the payment succesfully.
     * @returns {undefined}
     */
    completePayment: () => {
      content.document.getElementById("pay").click();
    },

    setSecurityCode: ({securityCode}) => {
      // Waive the xray to access the untrusted `securityCodeInput` property
      let picker = Cu.waiveXrays(content.document.querySelector("payment-method-picker"));
      // Unwaive to access the ChromeOnly `setUserInput` API.
      // setUserInput dispatches changes events.
      Cu.unwaiveXrays(picker.securityCodeInput).setUserInput(securityCode);
    },
  },

  DialogContentUtils: {
    waitForState: async (content, stateCheckFn, msg) => {
      const {
        ContentTaskUtils,
      } = ChromeUtils.import("resource://testing-common/ContentTaskUtils.jsm", {});
      let {requestStore} = Cu.waiveXrays(content.document.querySelector("payment-dialog"));
      await ContentTaskUtils.waitForCondition(() => stateCheckFn(requestStore.getState()), msg);
      return requestStore.getState();
    },
  },

  /**
   * Common PaymentMethodData for testing
   */
  MethodData: {
    basicCard: {
      supportedMethods: "basic-card",
    },
    bobPay: {
      supportedMethods: "https://www.example.com/bobpay",
    },
  },

  /**
   * Common PaymentDetailsInit for testing
   */
  Details: {
    total60USD: {
      total: {
        label: "Total due",
        amount: { currency: "USD", value: "60.00" },
      },
    },
    total60EUR: {
      total: {
        label: "Total due",
        amount: { currency: "EUR", value: "75.00" },
      },
    },
    twoDisplayItems: {
      total: {
        label: "Total due",
        amount: { currency: "USD", value: "32.00" },
      },
      displayItems: [
        {
          label: "First",
          amount: { currency: "USD", value: "1" },
        },
        {
          label: "Second",
          amount: { currency: "USD", value: "2" },
        },
      ],
    },
    twoShippingOptions: {
      total: {
        label: "Total due",
        amount: { currency: "USD", value: "2.00" },
      },
      shippingOptions: [
        {
          id: "1",
          label: "Meh Unreliable Shipping",
          amount: { currency: "USD", value: "1" },
        },
        {
          id: "2",
          label: "Premium Slow Shipping",
          amount: { currency: "USD", value: "2" },
          selected: true,
        },
      ],
    },
    twoShippingOptionsEUR: {
      total: {
        label: "Total due",
        amount: { currency: "EUR", value: "1.75" },
      },
      shippingOptions: [
        {
          id: "1",
          label: "Sloth Shipping",
          amount: { currency: "EUR", value: "1.01" },
        },
        {
          id: "2",
          label: "Velociraptor Shipping",
          amount: { currency: "EUR", value: "63545.65" },
          selected: true,
        },
      ],
    },
    bobPayPaymentModifier: {
      total: {
        label: "Total due",
        amount: { currency: "USD", value: "2.00" },
      },
      displayItems: [
        {
          label: "First",
          amount: { currency: "USD", value: "1.75" },
        },
        {
          label: "Second",
          amount: { currency: "USD", value: "0.25" },
        },
      ],
      modifiers: [
        {
          additionalDisplayItems: [
            {
              label: "Credit card fee",
              amount: { currency: "USD", value: "0.50" },
            },
          ],
          supportedMethods: "basic-card",
          total: {
            label: "Total due",
            amount: { currency: "USD", value: "2.50" },
          },
          data: {
            supportedTypes: "credit",
          },
        },
        {
          additionalDisplayItems: [
            {
              label: "Bob-pay fee",
              amount: { currency: "USD", value: "1.50" },
            },
          ],
          supportedMethods: "https://www.example.com/bobpay",
          total: {
            label: "Total due",
            amount: { currency: "USD", value: "3.50" },
          },
        },
      ],
    },
  },

  Options: {
    requestShippingOption: {
      requestShipping: true,
    },
    requestPayerNameAndEmail: {
      requestPayerName: true,
      requestPayerEmail: true,
    },
    requestPayerNameEmailAndPhone: {
      requestPayerName: true,
      requestPayerEmail: true,
      requestPayerPhone: true,
    },
  },

  Addresses: {
    TimBL: {
      "given-name": "Timothy",
      "additional-name": "John",
      "family-name": "Berners-Lee",
      organization: "World Wide Web Consortium",
      "street-address": "32 Vassar Street\nMIT Room 32-G524",
      "address-level2": "Cambridge",
      "address-level1": "MA",
      "postal-code": "02139",
      country: "US",
      tel: "+16172535702",
      email: "timbl@example.org",
    },
    TimBL2: {
      "given-name": "Timothy",
      "additional-name": "John",
      "family-name": "Berners-Lee",
      organization: "World Wide Web Consortium",
      "street-address": "1 Pommes Frittes Place",
      "address-level2": "Berlin",
      "address-level1": "BE",
      "postal-code": "02138",
      country: "DE",
      tel: "+16172535702",
      email: "timbl@example.org",
    },
  },

  BasicCards: {
    JohnDoe: {
      "cc-exp-month": 1,
      "cc-exp-year": 9999,
      "cc-name": "John Doe",
      "cc-number": "999999999999",
    },
  },
};
