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
    addCompletionHandler: async ({result, delayMs = 0}) => {
      let response = await content.showPromise;
      let completeException;

      // delay the given # milliseconds
      await new Promise(resolve => content.setTimeout(resolve, delayMs));

      try {
        await response.complete(result);
      } catch (ex) {
        completeException = {
          name: ex.name,
          message: ex.message,
        };
      }
      return {
        completeException,
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
    getShippingOptions: () => {
      let picker = content.document.querySelector("shipping-option-picker");
      let popupBox = Cu.waiveXrays(picker).dropdown.popupBox;
      let selectedOptionIndex = popupBox.selectedIndex;
      let selectedOption = Cu.waiveXrays(picker).dropdown.selectedOption;
      return {
        optionCount: popupBox.children.length,
        selectedOptionIndex,
        selectedOptionID: selectedOption.getAttribute("value"),
        selectedOptionLabel: selectedOption.getAttribute("label"),
        selectedOptionCurrency: selectedOption.getAttribute("amount-currency"),
        selectedOptionValue: selectedOption.getAttribute("amount-value"),
      };
    },

    getShippingAddresses: () => {
      let doc = content.document;
      let addressPicker =
        doc.querySelector("address-picker[selected-state-key='selectedShippingAddress']");
      let popupBox = Cu.waiveXrays(addressPicker).dropdown.popupBox;
      let options = Array.from(popupBox.children).map(option => {
        return {
          guid: option.getAttribute("guid"),
          country: option.getAttribute("country"),
          selected: option.selected,
        };
      });
      let selectedOptionIndex = options.findIndex(item => item.selected);
      return {
        selectedOptionIndex,
        options,
      };
    },

    selectShippingAddressByCountry: (country) => {
      let doc = content.document;
      let addressPicker =
        doc.querySelector("address-picker[selected-state-key='selectedShippingAddress']");
      let select = Cu.waiveXrays(addressPicker).dropdown.popupBox;
      let option = select.querySelector(`[country="${country}"]`);
      select.focus();
      // eslint-disable-next-line no-undef
      EventUtils.synthesizeKey(option.label, {}, content.window);
    },

    selectShippingAddressByGuid: guid => {
      let doc = content.document;
      let addressPicker =
        doc.querySelector("address-picker[selected-state-key='selectedShippingAddress']");
      let select = Cu.waiveXrays(addressPicker).dropdown.popupBox;
      let option = select.querySelector(`[guid="${guid}"]`);
      select.focus();
      // eslint-disable-next-line no-undef
      EventUtils.synthesizeKey(option.label, {}, content.window);
    },

    selectShippingOptionById: value => {
      let doc = content.document;
      let optionPicker =
        doc.querySelector("shipping-option-picker");
      let select = Cu.waiveXrays(optionPicker).dropdown.popupBox;
      let option = select.querySelector(`[value="${value}"]`);
      select.focus();
      // eslint-disable-next-line no-undef
      EventUtils.synthesizeKey(option.textContent, {}, content.window);
    },

    /**
     * Click the primary button for the current page
     *
     * Don't await on this method from a ContentTask when expecting the dialog to close
     *
     * @returns {undefined}
     */
    clickPrimaryButton: () => {
      let {requestStore} = Cu.waiveXrays(content.document.querySelector("payment-dialog"));
      let {page} = requestStore.getState();
      let button = content.document.querySelector(`#${page.id} button.primary`);
      button.click();
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
     *
     * Don't await on this task since the cancel can close the dialog before
     * ContentTask can resolve the promise.
     *
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

    getCurrentState: async (content) => {
      let {requestStore} = Cu.waiveXrays(content.document.querySelector("payment-dialog"));
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
    total2USD: {
      total: {
        label: "Total due",
        amount: { currency: "USD", value: "2.00" },
      },
    },
    total32USD: {
      total: {
        label: "Total due",
        amount: { currency: "USD", value: "32.00" },
      },
    },
    total60USD: {
      total: {
        label: "Total due",
        amount: { currency: "USD", value: "60.00" },
      },
    },
    total1pt75EUR: {
      total: {
        label: "Total due",
        amount: { currency: "EUR", value: "1.75" },
      },
    },
    total60EUR: {
      total: {
        label: "Total due",
        amount: { currency: "EUR", value: "75.00" },
      },
    },
    twoDisplayItems: {
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
    twoDisplayItemsEUR: {
      displayItems: [
        {
          label: "First",
          amount: { currency: "EUR", value: "0.85" },
        },
        {
          label: "Second",
          amount: { currency: "EUR", value: "1.70" },
        },
      ],
    },
    twoShippingOptions: {
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
    noShippingOptions: {
      shippingOptions: [],
    },
    bobPayPaymentModifier: {
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
    additionalDisplayItemsEUR: {
      modifiers: [
        {
          additionalDisplayItems: [
            {
              label: "Handling fee",
              amount: { currency: "EUR", value: "1.00" },
            },
          ],
          supportedMethods: "basic-card",
          total: {
            label: "Total due",
            amount: { currency: "EUR", value: "2.50" },
          },
        },
      ],
    },
    noError: {
      error: "",
    },
    genericShippingError: {
      error: "Cannot ship with option 1 on days that end with Y",
    },
    fieldSpecificErrors: {
      error: "There are errors related to specific parts of the address",
      shippingAddressErrors: {
        addressLine: "Can only ship to ROADS, not DRIVES, BOULEVARDS, or STREETS",
        city: "Can only ship to CITIES, not TOWNSHIPS or VILLAGES",
        country: "Can only ship to USA, not CA",
        organization: "Can only ship to CORPORATIONS, not CONSORTIUMS",
        phone: "Only allowed to ship to area codes that start with 9",
        postalCode: "Only allowed to ship to postalCodes that start with 0",
        recipient: "Can only ship to names that start with J",
        region: "Can only ship to regions that start with M",
      },
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
      "additional-name": "Johann",
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
    /* Used as a temporary (not persisted in autofill storage) address in tests */
    Temp: {
      "given-name": "Temp",
      "family-name": "McTempFace",
      "organization": "Temps Inc.",
      "street-address": "1a Temporary Ave.",
      "address-level2": "Temp Town",
      "address-level1": "CA",
      "postal-code": "31337",
      "country": "US",
      "tel": "+15032541000",
      "email": "tempie@example.com",
    },
  },

  BasicCards: {
    JohnDoe: {
      "cc-exp-month": 1,
      "cc-exp-year": (new Date()).getFullYear() + 9,
      "cc-name": "John Doe",
      "cc-number": "4111111111111111",
    },
    JaneMasterCard: {
      "cc-exp-month": 12,
      "cc-exp-year": (new Date()).getFullYear() + 9,
      "cc-name": "Jane McMaster-Card",
      "cc-number": "5555555555554444",
    },
    Temp: {
      "cc-exp-month": 12,
      "cc-exp-year": (new Date()).getFullYear() + 9,
      "cc-name": "Temp Name",
      "cc-number": "5105105105105100",
    },
  },
};
