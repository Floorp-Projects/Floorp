"use strict";

/**
 * Basic checks to ensure that helpers constructing responses map their
 * destructured arguments properly to the `init` methods. Full testing of the init
 * methods is left to the DOM code.
 */

const DIALOG_WRAPPER_URI = "chrome://payments/content/paymentDialogWrapper.js";
let dialogGlobal = {};
Services.scriptloader.loadSubScript(DIALOG_WRAPPER_URI, dialogGlobal);

add_task(async function test_createBasicCardResponseData_basic() {
  let expected = {
    cardholderName: "John Smith",
    cardNumber: "1234567890",
    expiryMonth: "01",
    expiryYear: "2017",
    cardSecurityCode: "0123",
  };
  let actual = dialogGlobal.paymentDialogWrapper.createBasicCardResponseData(
    expected
  );
  Assert.equal(
    actual.cardholderName,
    expected.cardholderName,
    "Check cardholderName"
  );
  Assert.equal(actual.cardNumber, expected.cardNumber, "Check cardNumber");
  Assert.equal(actual.expiryMonth, expected.expiryMonth, "Check expiryMonth");
  Assert.equal(actual.expiryYear, expected.expiryYear, "Check expiryYear");
  Assert.equal(
    actual.cardSecurityCode,
    expected.cardSecurityCode,
    "Check cardSecurityCode"
  );
});

add_task(async function test_createBasicCardResponseData_minimal() {
  let expected = {
    cardNumber: "1234567890",
  };
  let actual = dialogGlobal.paymentDialogWrapper.createBasicCardResponseData(
    expected
  );
  info(actual.cardNumber);
  Assert.equal(actual.cardNumber, expected.cardNumber, "Check cardNumber");
});

add_task(async function test_createBasicCardResponseData_withoutNumber() {
  let data = {
    cardholderName: "John Smith",
    expiryMonth: "01",
    expiryYear: "2017",
    cardSecurityCode: "0123",
  };
  Assert.throws(
    () => dialogGlobal.paymentDialogWrapper.createBasicCardResponseData(data),
    /NS_ERROR_FAILURE/,
    "Check cardNumber is required"
  );
});

function checkAddress(actual, expected) {
  for (let [propName, propVal] of Object.entries(expected)) {
    if (propName == "addressLines") {
      // Note the singular vs. plural here.
      Assert.equal(
        actual.addressLine.length,
        propVal.length,
        "Check number of address lines"
      );
      for (let [i, line] of expected.addressLines.entries()) {
        Assert.equal(
          actual.addressLine.queryElementAt(i, Ci.nsISupportsString).data,
          line,
          `Check ${propName} line ${i}`
        );
      }
      continue;
    }
    Assert.equal(actual[propName], propVal, `Check ${propName}`);
  }
}

add_task(async function test_createPaymentAddress_minimal() {
  let data = {
    country: "CA",
  };
  let actual = dialogGlobal.paymentDialogWrapper.createPaymentAddress(data);
  checkAddress(actual, data);
});

add_task(async function test_createPaymentAddress_basic() {
  let data = {
    country: "CA",
    addressLines: ["123 Sesame Street", "P.O. Box ABC"],
    region: "ON",
    city: "Delhi",
    dependentLocality: "N/A",
    postalCode: "94041",
    sortingCode: "1234",
    organization: "Mozilla Corporation",
    recipient: "John Smith",
    phone: "+15195555555",
  };
  let actual = dialogGlobal.paymentDialogWrapper.createPaymentAddress(data);
  checkAddress(actual, data);
});

add_task(async function test_createShowResponse_basic() {
  let requestId = "876hmbvfd45hb";
  dialogGlobal.paymentDialogWrapper.request = {
    requestId,
  };

  let cardData = {
    cardholderName: "John Smith",
    cardNumber: "1234567890",
    expiryMonth: "01",
    expiryYear: "2099",
    cardSecurityCode: "0123",
  };
  let methodData = dialogGlobal.paymentDialogWrapper.createBasicCardResponseData(
    cardData
  );

  let responseData = {
    acceptStatus: Ci.nsIPaymentActionResponse.PAYMENT_ACCEPTED,
    methodName: "basic-card",
    methodData,
    payerName: "My Name",
    payerEmail: "my.email@example.com",
    payerPhone: "+15195555555",
  };
  let actual = dialogGlobal.paymentDialogWrapper.createShowResponse(
    responseData
  );
  for (let [propName, propVal] of Object.entries(actual)) {
    if (typeof propVal != "string") {
      continue;
    }
    if (propName == "requestId") {
      Assert.equal(propVal, requestId, `Check ${propName}`);
      continue;
    }
    Assert.equal(propVal, responseData[propName], `Check ${propName}`);
  }
});
