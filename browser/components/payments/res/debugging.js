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
    let options = state.request.paymentOptions;
    let checkboxes = document.querySelectorAll("#paymentOptions input[type='checkbox']");
    for (let input of checkboxes) {
      if (options.hasOwnProperty(input.name)) {
        input.checked = options[input.name];
      }
    }
  },
};

requestStore.subscribe(paymentOptionsUpdater);

let REQUEST_1 = {
  tabId: 9,
  topLevelPrincipal: {URI: {displayHost: "tschaeff.github.io"}},
  requestId: "3797081f-a96b-c34b-a58b-1083c6e66e25",
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
    shippingOptions: [
      {
        id: "123",
        label: "Fast",
        amount: {
          currency: "USD",
          value: 10,
        },
        selected: false,
      },
      {
        id: "456",
        label: "Faster (default)",
        amount: {
          currency: "USD",
          value: 20,
        },
        selected: true,
      },
    ],
    modifiers: null,
    error: "",
  },
  paymentOptions: {
    requestPayerName: false,
    requestPayerEmail: false,
    requestPayerPhone: false,
    requestShipping: true,
    shippingType: "shipping",
  },
  shippingOption: "456",
};

let REQUEST_2 = {
  tabId: 9,
  topLevelPrincipal: {URI: {displayHost: "example.com"}},
  requestId: "3797081f-a96b-c34b-a58b-1083c6e66e25",
  paymentMethods: [],
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
          value: 10,
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
    "guid": "48bnds6854t",
    "name": "Mr. Foo",
    "postal-code": "90210",
    "street-address": "123 Sesame Street,\nApt 40",
    "tel": "+1 519 555-5555",
  },
  "68gjdh354j": {
    "address-level1": "CA",
    "address-level2": "Mountain View",
    "country": "US",
    "guid": "68gjdh354j",
    "name": "Mrs. Bar",
    "postal-code": "94041",
    "street-address": "P.O. Box 123",
    "tel": "+1 650 555-5555",
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
    "guid": "a9e830667189",
    "tel": "+19871234567",
    "name": "Bob Smith",
  },
  "72a15aed206d": {
    "street-address": "1 New St",
    "address-level2": "York",
    "address-level1": "SC",
    "postal-code": "29745",
    "country": "US",
    "guid": "72a15aed206d",
    "tel": "+19871234567",
    "name": "Mary Sue",
    "address-line1": "1 New St",
  },
  "2b4dce0fbc1f": {
    "street-address": "123 Park St",
    "address-level2": "Springfield",
    "address-level1": "OR",
    "postal-code": "97403",
    "country": "US",
    "email": "rita@foo.com",
    "guid": "2b4dce0fbc1f",
    "name": "Rita Foo",
    "address-line1": "123 Park St",
  },
  "46b2635a5b26": {
    "street-address": "432 Another St",
    "address-level2": "Springfield",
    "address-level1": "OR",
    "postal-code": "97402",
    "country": "US",
    "email": "rita@foo.com",
    "guid": "46b2635a5b26",
    "name": "Rita Foo",
    "address-line1": "432 Another St",
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
    "timeLastUsed": 0,
    "timesUsed": 0,
    "cc-name": "John Smith",
    "cc-exp-month": 6,
    "cc-exp-year": 2024,
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
    "timeLastUsed": 0,
    "timesUsed": 0,
    "cc-name": "Jane Doe",
    "cc-exp-month": 5,
    "cc-exp-year": 2023,
    "cc-given-name": "Jane",
    "cc-additional-name": "",
    "cc-family-name": "Doe",
    "cc-exp": "2023-05",
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

  setAddresses1() {
    paymentDialog.setStateFromParent({savedAddresses: ADDRESSES_1});
  },

  setDupesAddresses() {
    paymentDialog.setStateFromParent({savedAddresses: DUPED_ADDRESSES});
  },

  setBasicCards1() {
    paymentDialog.setStateFromParent({savedBasicCards: BASIC_CARDS_1});
  },

  setChangesAllowed() {
    requestStore.setState({
      changesPrevented: false,
    });
  },

  setChangesPrevented() {
    requestStore.setState({
      changesPrevented: true,
    });
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

  setShippingError() {
    let request = Object.assign({}, requestStore.getState().request);
    request.paymentDetails = Object.assign({}, requestStore.getState().request.paymentDetails);
    request.paymentDetails.error = "Error!";
    request.paymentDetails.shippingOptions = [];
    requestStore.setState({
      request,
    });
  },

  setStateDefault() {
    requestStore.setState({
      completionState: "initial",
    });
  },

  setStateProcessing() {
    requestStore.setState({
      completionState: "processing",
    });
  },

  setStateSuccess() {
    requestStore.setState({
      completionState: "success",
    });
  },

  setStateFail() {
    requestStore.setState({
      completionState: "fail",
    });
  },

  setStateUnknown() {
    requestStore.setState({
      completionState: "unknown",
    });
  },
};

window.addEventListener("click", function onButtonClick(evt) {
  let id = evt.target.id;
  if (!id || typeof(buttonActions[id]) != "function") {
    return;
  }

  buttonActions[id]();
});

window.addEventListener("DOMContentLoaded", function onDCL() {
  if (window.location.protocol == "resource:") {
    // Only show the debug frame button if we're running from a resource URI
    // so it doesn't show during development over file: or http: since it won't work.
    // Note that the button still won't work if resource://payments/paymentRequest.xhtml
    // is manually loaded in a tab but will be shown.
    document.getElementById("debugFrame").hidden = false;
  }
});
