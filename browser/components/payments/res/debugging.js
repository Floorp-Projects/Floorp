/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const paymentDialog = window.parent.document.querySelector("payment-dialog");
// The requestStore should be manipulated for most changes but autofill storage changes
// happen through setStateFromParent which includes some consistency checks.
const requestStore = paymentDialog.requestStore;

// keep the payment options checkboxes in sync w. actual state
const paymentOptionsUpdater = {
  stateChangeCallback(state) {
    this.render(state);
  },
  render(state) {
    let {
      completeStatus,
      paymentOptions,
    } = state.request;

    document.getElementById("setChangesPrevented").checked = state.changesPrevented;

    let paymentOptionInputs = document.querySelectorAll("#paymentOptions input[type='checkbox']");
    for (let input of paymentOptionInputs) {
      if (paymentOptions.hasOwnProperty(input.name)) {
        input.checked = paymentOptions[input.name];
      }
    }

    let completeStatusInputs = document
                                 .querySelectorAll("input[type='radio'][name='setCompleteStatus']");
    for (let input of completeStatusInputs) {
      input.checked = input.value == completeStatus;
    }
  },
};

let REQUEST_1 = {
  tabId: 9,
  topLevelPrincipal: {URI: {displayHost: "debugging.example.com"}},
  requestId: "3797081f-a96b-c34b-a58b-1083c6e66e25",
  completeStatus: "",
  paymentMethods: [],
  paymentDetails: {
    id: "",
    totalItem: {label: "Demo total", amount: {currency: "EUR", value: "1.00"}, pending: false},
    displayItems: [
      {
        label: "Square",
        amount: {
          currency: "USD",
          value: "5",
        },
      },
    ],
    payer: {},
    paymentMethod: {},
    shippingAddressErrors: {},
    shippingOptions: [
      {
        id: "std",
        label: "Standard (3-5 business days)",
        amount: {
          currency: "USD",
          value: 10,
        },
        selected: false,
      },
      {
        id: "super-slow",
        // Long to test truncation
        label: "Ssssssssuuuuuuuuupppppeeeeeeerrrrr sssssllllllloooooowwwwww",
        amount: {
          currency: "USD",
          value: 1.50,
        },
        selected: true,
      },
    ],
    modifiers: null,
    error: "",
  },
  paymentOptions: {
    requestPayerName: true,
    requestPayerEmail: false,
    requestPayerPhone: false,
    requestShipping: true,
    shippingType: "shipping",
  },
  shippingOption: "std",
};

let REQUEST_2 = {
  tabId: 9,
  topLevelPrincipal: {URI: {displayHost: "example.com"}},
  requestId: "3797081f-a96b-c34b-a58b-1083c6e66e25",
  completeStatus: "",
  paymentMethods: [
    {
      "supportedMethods": "basic-card",
      "data": {
        "supportedNetworks": [
          "amex",
          "discover",
          "mastercard",
          "visa",
        ],
      },
    },
  ],
  paymentDetails: {
    id: "",
    totalItem: {label: "", amount: {currency: "CAD", value: "25.75"}, pending: false},
    displayItems: [
      {
        label: "Triangle",
        amount: {
          currency: "CAD",
          value: "3",
        },
      },
      {
        label: "Circle",
        amount: {
          currency: "EUR",
          value: "10.50",
        },
      },
      {
        label: "Tax",
        type: "tax",
        amount: {
          currency: "USD",
          value: "1.50",
        },
      },
    ],
    payer: {},
    paymentMethod: {},
    shippingAddressErrors: {},
    shippingOptions: [
      {
        id: "123",
        label: "Fast (default)",
        amount: {
          currency: "USD",
          value: 10,
        },
        selected: true,
      },
      {
        id: "947",
        label: "Slow",
        amount: {
          currency: "USD",
          value: 1,
        },
        selected: false,
      },
    ],
    modifiers: [
      {
        supportedMethods: "basic-card",
        total: {
          label: "Total",
          amount: {
            currency: "CAD",
            value: "28.75",
          },
          pending: false,
        },
        additionalDisplayItems: [
          {
            label: "Credit card fee",
            amount: {
              currency: "CAD",
              value: "1.50",
            },
          },
        ],
        data: {
          supportedTypes: "credit",
        },
      },
    ],
    error: "",
  },
  paymentOptions: {
    requestPayerName: false,
    requestPayerEmail: false,
    requestPayerPhone: false,
    requestShipping: true,
    shippingType: "shipping",
  },
  shippingOption: "123",
};

let ADDRESSES_1 = {
  "48bnds6854t": {
    "address-level1": "MI",
    "address-level2": "Some City",
    "country": "US",
    "email": "foo@bar.com",
    "family-name": "Smith",
    "given-name": "John",
    "guid": "48bnds6854t",
    "name": "John Smith",
    "postal-code": "90210",
    "street-address": "123 Sesame Street,\nApt 40",
    "tel": "+1 519 555-5555",
    timeLastUsed: 50000,
  },
  "68gjdh354j": {
    "additional-name": "Z.",
    "address-level1": "CA",
    "address-level2": "Mountain View",
    "country": "US",
    "family-name": "Doe",
    "given-name": "Jane",
    "guid": "68gjdh354j",
    "name": "Jane Z. Doe",
    "postal-code": "94041",
    "street-address": "P.O. Box 123",
    "tel": "+1 650 555-5555",
    timeLastUsed: 30000,
  },
  "abcde12345": {
    "address-level2": "Mountain View",
    "country": "US",
    "family-name": "Fields",
    "given-name": "Mrs.",
    "guid": "abcde12345",
    "name": "Mrs. Fields",
    timeLastUsed: 70000,
  },
  "german1": {
    "additional-name": "Y.",
    "address-level1": "",
    "address-level2": "Berlin",
    "country": "DE",
    "email": "de@example.com",
    "family-name": "Mouse",
    "given-name": "Anon",
    "guid": "german1",
    "name": "Anon Y. Mouse",
    "organization": "Mozilla",
    "postal-code": "10997",
    "street-address": "Schlesische Str. 27",
    "tel": "+49 30 983333002",
    timeLastUsed: 10000,
  },
  "missing-country": {
    "address-level1": "ON",
    "address-level2": "Toronto",
    "family-name": "Bogard",
    "given-name": "Kristin",
    "guid": "missing-country",
    "name": "Kristin Bogard",
    "postal-code": "H0H 0H0",
    "street-address": "123 Yonge Street\nSuite 2300",
    "tel": "+1 416 555-5555",
    timeLastUsed: 90000,
  },
};

let DUPED_ADDRESSES = {
  "a9e830667189": {
    "street-address": "Unit 1\n1505 Northeast Kentucky Industrial Parkway \n",
    "address-level2": "Greenup",
    "address-level1": "KY",
    "postal-code": "41144",
    "country": "US",
    "email": "bob@example.com",
    "family-name": "Smith",
    "given-name": "Bob",
    "guid": "a9e830667189",
    "tel": "+19871234567",
    "name": "Bob Smith",
    timeLastUsed: 10001,
  },
  "72a15aed206d": {
    "street-address": "1 New St",
    "address-level2": "York",
    "address-level1": "SC",
    "postal-code": "29745",
    "country": "US",
    "given-name": "Mary Sue",
    "guid": "72a15aed206d",
    "tel": "+19871234567",
    "name": "Mary Sue",
    "address-line1": "1 New St",
    timeLastUsed: 10009,
  },
  "2b4dce0fbc1f": {
    "street-address": "123 Park St",
    "address-level2": "Springfield",
    "address-level1": "OR",
    "postal-code": "97403",
    "country": "US",
    "email": "rita@foo.com",
    "family-name": "Foo",
    "given-name": "Rita",
    "guid": "2b4dce0fbc1f",
    "name": "Rita Foo",
    "address-line1": "123 Park St",
    timeLastUsed: 10005,
  },
  "46b2635a5b26": {
    "street-address": "432 Another St",
    "address-level2": "Springfield",
    "address-level1": "OR",
    "postal-code": "97402",
    "country": "US",
    "email": "rita@foo.com",
    "family-name": "Foo",
    "given-name": "Rita",
    "guid": "46b2635a5b26",
    "name": "Rita Foo",
    "address-line1": "432 Another St",
    timeLastUsed: 10003,
  },
};

let BASIC_CARDS_1 = {
  "53f9d009aed2": {
    billingAddressGUID: "68gjdh354j",
    methodName: "basic-card",
    "cc-number": "************5461",
    "guid": "53f9d009aed2",
    "version": 1,
    "timeCreated": 1505240896213,
    "timeLastModified": 1515609524588,
    "timeLastUsed": 10000,
    "timesUsed": 0,
    "cc-name": "John Smith",
    "cc-exp-month": 6,
    "cc-exp-year": 2024,
    "cc-type": "visa",
    "cc-given-name": "John",
    "cc-additional-name": "",
    "cc-family-name": "Smith",
    "cc-exp": "2024-06",
  },
  "9h5d4h6f4d1s": {
    methodName: "basic-card",
    "cc-number": "************0954",
    "guid": "9h5d4h6f4d1s",
    "version": 1,
    "timeCreated": 1517890536491,
    "timeLastModified": 1517890564518,
    "timeLastUsed": 50000,
    "timesUsed": 0,
    "cc-name": "Jane Doe",
    "cc-exp-month": 5,
    "cc-exp-year": 2023,
    "cc-type": "mastercard",
    "cc-given-name": "Jane",
    "cc-additional-name": "",
    "cc-family-name": "Doe",
    "cc-exp": "2023-05",
  },
  "123456789abc": {
    methodName: "basic-card",
    "cc-number": "************1234",
    "guid": "123456789abc",
    "version": 1,
    "timeCreated": 1517890536491,
    "timeLastModified": 1517890564518,
    "timeLastUsed": 90000,
    "timesUsed": 0,
    "cc-name": "Jane Fields",
    "cc-given-name": "Jane",
    "cc-additional-name": "",
    "cc-family-name": "Fields",
    "cc-type": "discover",
  },
  "amex-card": {
    methodName: "basic-card",
    billingAddressGUID: "68gjdh354j",
    "cc-number": "************1941",
    "guid": "amex-card",
    "version": 1,
    "timeCreated": 1517890536491,
    "timeLastModified": 1517890564518,
    "timeLastUsed": 70000,
    "timesUsed": 0,
    "cc-name": "Capt America",
    "cc-given-name": "Capt",
    "cc-additional-name": "",
    "cc-family-name": "America",
    "cc-type": "amex",
    "cc-exp-month": 6,
    "cc-exp-year": 2023,
    "cc-exp": "2023-06",
  },
  "missing-cc-name": {
    methodName: "basic-card",
    "cc-number": "************8563",
    "guid": "missing-cc-name",
    "version": 1,
    "timeCreated": 1517890536491,
    "timeLastModified": 1517890564518,
    "timeLastUsed": 30000,
    "timesUsed": 0,
    "cc-exp-month": 8,
    "cc-exp-year": 2024,
    "cc-exp": "2024-08",
  },
};

let buttonActions = {
  debugFrame() {
    let event = new CustomEvent("paymentContentToChrome", {
      bubbles: true,
      detail: {
        messageType: "debugFrame",
      },
    });
    document.dispatchEvent(event);
  },

  delete1Address() {
    let savedAddresses = Object.assign({}, requestStore.getState().savedAddresses);
    delete savedAddresses[Object.keys(savedAddresses)[0]];
    // Use setStateFromParent since it ensures there is no dangling
    // `selectedShippingAddress` foreign key (FK) reference.
    paymentDialog.setStateFromParent({
      savedAddresses,
    });
  },

  delete1Card() {
    let savedBasicCards = Object.assign({}, requestStore.getState().savedBasicCards);
    delete savedBasicCards[Object.keys(savedBasicCards)[0]];
    // Use setStateFromParent since it ensures there is no dangling
    // `selectedPaymentCard` foreign key (FK) reference.
    paymentDialog.setStateFromParent({
      savedBasicCards,
    });
  },

  logState() {
    let state = requestStore.getState();
    // eslint-disable-next-line no-console
    console.log(state);
    dump(`${JSON.stringify(state, null, 2)}\n`);
  },

  refresh() {
    window.parent.location.reload(true);
  },

  rerender() {
    requestStore.setState({});
  },

  saveVisibleForm() {
    // Bypasses field validation which is useful to test error handling.
    paymentDialog.querySelector("#main-container > .page:not([hidden])").saveRecord();
  },

  setAddresses1() {
    paymentDialog.setStateFromParent({savedAddresses: ADDRESSES_1});
  },

  setDupesAddresses() {
    paymentDialog.setStateFromParent({savedAddresses: DUPED_ADDRESSES});
  },

  setBasicCards1() {
    paymentDialog.setStateFromParent({savedBasicCards: BASIC_CARDS_1});
  },

  setBasicCardErrors() {
    let request = Object.assign({}, requestStore.getState().request);
    request.paymentDetails = Object.assign({}, requestStore.getState().request.paymentDetails);
    request.paymentDetails.paymentMethod = {
      cardNumber: "",
      cardholderName: "",
      cardSecurityCode: "",
      expiryMonth: "",
      expiryYear: "",
      billingAddress: {
        addressLine: "Can only buy from ROADS, not DRIVES, BOULEVARDS, or STREETS",
        city: "Can only buy from CITIES, not TOWNSHIPS or VILLAGES",
        country: "Can only buy from US, not CA",
        organization: "Can only buy from CORPORATIONS, not CONSORTIUMS",
        phone: "Only allowed to buy from area codes that start with 9",
        postalCode: "Only allowed to buy from postalCodes that start with 0",
        recipient: "Can only buy from names that start with J",
        region: "Can only buy from regions that start with M",
      },
    };
    requestStore.setState({
      request,
    });
  },


  setChangesPrevented(evt) {
    requestStore.setState({
      changesPrevented: evt.target.checked,
    });
  },

  setCompleteStatus() {
    let input = document.querySelector("[name='setCompleteStatus']:checked");
    let completeStatus = input.value;
    let request = requestStore.getState().request;
    paymentDialog.setStateFromParent({
      request: Object.assign({}, request, { completeStatus }),
    });
  },

  setPayerErrors() {
    let request = Object.assign({}, requestStore.getState().request);
    request.paymentDetails = Object.assign({}, requestStore.getState().request.paymentDetails);
    request.paymentDetails.payer = {
      email: "Only @mozilla.com emails are supported",
      name: "Payer name must start with M",
      phone: "Payer area codes must start with 1",
    };
    requestStore.setState({
      request,
    });
  },

  setPaymentOptions() {
    let options = {};
    let checkboxes = document.querySelectorAll("#paymentOptions input[type='checkbox']");
    for (let input of checkboxes) {
      options[input.name] = input.checked;
    }
    let req = Object.assign({}, requestStore.getState().request, {
      paymentOptions: options,
    });
    requestStore.setState({ request: req });
  },

  setRequest1() {
    paymentDialog.setStateFromParent({request: REQUEST_1});
  },

  setRequest2() {
    paymentDialog.setStateFromParent({request: REQUEST_2});
  },

  setRequestPayerName() {
    buttonActions.setPaymentOptions();
  },
  setRequestPayerEmail() {
    buttonActions.setPaymentOptions();
  },
  setRequestPayerPhone() {
    buttonActions.setPaymentOptions();
  },
  setRequestShipping() {
    buttonActions.setPaymentOptions();
  },

  setShippingError() {
    let request = Object.assign({}, requestStore.getState().request);
    request.paymentDetails = Object.assign({}, requestStore.getState().request.paymentDetails);
    request.paymentDetails.error = "Shipping Error!";
    request.paymentDetails.shippingOptions = [];
    requestStore.setState({
      request,
    });
  },

  setShippingAddressErrors() {
    let request = Object.assign({}, requestStore.getState().request);
    request.paymentDetails = Object.assign({}, requestStore.getState().request.paymentDetails);
    request.paymentDetails.shippingAddressErrors = {
      addressLine: "Can only ship to ROADS, not DRIVES, BOULEVARDS, or STREETS",
      city: "Can only ship to CITIES, not TOWNSHIPS or VILLAGES",
      country: "Can only ship to USA, not CA",
      organization: "Can only ship to CORPORATIONS, not CONSORTIUMS",
      phone: "Only allowed to ship to area codes that start with 9",
      postalCode: "Only allowed to ship to postalCodes that start with 0",
      recipient: "Can only ship to names that start with J",
      region: "Can only ship to regions that start with M",
    };
    requestStore.setState({
      request,
    });
  },

  toggleDirectionality() {
    let body = paymentDialog.ownerDocument.body;
    body.dir = body.dir == "rtl" ? "ltr" : "rtl";
  },

  toggleBranding() {
    for (let container of paymentDialog.querySelectorAll("accepted-cards")) {
      container.classList.toggle("branded");
    }
  },
};

window.addEventListener("click", function onButtonClick(evt) {
  let id = evt.target.id || evt.target.name;
  if (!id || typeof(buttonActions[id]) != "function") {
    return;
  }

  buttonActions[id](evt);
});

window.addEventListener("DOMContentLoaded", function onDCL() {
  if (window.location.protocol == "resource:") {
    // Only show the debug frame button if we're running from a resource URI
    // so it doesn't show during development over file: or http: since it won't work.
    // Note that the button still won't work if resource://payments/paymentRequest.xhtml
    // is manually loaded in a tab but will be shown.
    document.getElementById("debugFrame").hidden = false;
  }

  requestStore.subscribe(paymentOptionsUpdater);
  paymentOptionsUpdater.render(requestStore.getState());
});
