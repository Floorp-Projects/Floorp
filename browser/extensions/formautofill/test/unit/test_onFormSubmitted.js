"use strict";

var FormAutofillContent;
add_setup(async () => {
  ({ FormAutofillContent } = ChromeUtils.import(
    "resource://autofill/FormAutofillContent.jsm"
  ));
});

const DEFAULT_TEST_DOC = `<form id="form1">
                      <input id="street-addr" autocomplete="street-address">
                      <select id="address-level1" autocomplete="address-level1">
                        <option value=""></option>
                        <option value="AL">Alabama</option>
                        <option value="AK">Alaska</option>
                        <option value="AP">Armed Forces Pacific</option>

                        <option value="ca">california</option>
                        <option value="AR">US-Arkansas</option>
                        <option value="US-CA">California</option>
                        <option value="CA">California</option>
                        <option value="US-AZ">US_Arizona</option>
                        <option value="Ariz">Arizonac</option>
                      </select>
                      <input id="city" autocomplete="address-level2">
                      <input id="country" autocomplete="country">
                      <input id="email" autocomplete="email">
                      <input id="tel" autocomplete="tel">
                      <input id="cc-name" autocomplete="cc-name">
                      <input id="cc-number" autocomplete="cc-number">
                      <input id="cc-exp-month" autocomplete="cc-exp-month">
                      <input id="cc-exp-year" autocomplete="cc-exp-year">
                      <select id="cc-type">
                        <option value="">Select</option>
                        <option value="visa">Visa</option>
                        <option value="mastercard">Master Card</option>
                        <option value="amex">American Express</option>
                      </select>
                      <input id="submit" type="submit">
                    </form>`;
const TARGET_ELEMENT_ID = "street-addr";

const TESTCASES = [
  {
    description:
      "Should not trigger address saving if the number of fields is less than 3",
    document: DEFAULT_TEST_DOC,
    targetElementId: TARGET_ELEMENT_ID,
    formValue: {
      "street-addr": "331 E. Evelyn Avenue",
      tel: "1-650-903-0800",
    },
    expectedResult: {
      formSubmission: false,
    },
  },
  {
    description: "Should not trigger credit card saving if number is empty",
    document: DEFAULT_TEST_DOC,
    targetElementId: TARGET_ELEMENT_ID,
    formValue: {
      "cc-name": "John Doe",
      "cc-exp-month": 12,
      "cc-exp-year": 2000,
    },
    expectedResult: {
      formSubmission: false,
    },
  },
  {
    description:
      "Should not trigger credit card saving if there is more than one cc-number field but less than four fields",
    document: `<form id="form1">
                <input id="cc-type" autocomplete="cc-type">
                <input id="cc-name" autocomplete="cc-name">
                <input id="cc-number1" maxlength="4">
                <input id="cc-number2" maxlength="4">
                <input id="cc-number3" maxlength="4">
                <input id="cc-exp-month" autocomplete="cc-exp-month">
                <input id="cc-exp-year" autocomplete="cc-exp-year">
                <input id="submit" type="submit">
              </form>
    `,
    targetElementId: "cc-name",
    formValue: {
      "cc-name": "John Doe",
      "cc-number1": "3714",
      "cc-number2": "4963",
      "cc-number3": "5398",
      "cc-exp-month": 12,
      "cc-exp-year": 2000,
      "cc-type": "amex",
    },
    expectedResult: {
      formSubmission: false,
    },
  },
  {
    description: "Trigger address saving",
    document: DEFAULT_TEST_DOC,
    targetElementId: TARGET_ELEMENT_ID,
    formValue: {
      "street-addr": "331 E. Evelyn Avenue",
      country: "US",
      tel: "1-650-903-0800",
    },
    expectedResult: {
      formSubmission: true,
      records: {
        address: [
          {
            guid: null,
            record: {
              "street-address": "331 E. Evelyn Avenue",
              "address-level1": "",
              "address-level2": "",
              country: "US",
              email: "",
              tel: "1-650-903-0800",
            },
            untouchedFields: [],
          },
        ],
        creditCard: [],
      },
    },
  },
  {
    description: "Trigger credit card saving",
    document: DEFAULT_TEST_DOC,
    targetElementId: "cc-type",
    formValue: {
      "cc-name": "John Doe",
      "cc-number": "5105105105105100",
      "cc-exp-month": 12,
      "cc-exp-year": 2000,
      "cc-type": "amex",
    },
    expectedResult: {
      formSubmission: true,
      records: {
        address: [],
        creditCard: [
          {
            guid: null,
            record: {
              "cc-name": "John Doe",
              "cc-number": "5105105105105100",
              "cc-exp-month": 12,
              "cc-exp-year": 2000,
              "cc-type": "amex",
            },
            untouchedFields: [],
          },
        ],
      },
    },
  },
  {
    description: "Trigger credit card saving using multiple cc-number fields",
    document: `<form id="form1">
                <input id="cc-type" autocomplete="cc-type">
                <input id="cc-name" autocomplete="cc-name">
                <input id="cc-number1" maxlength="4">
                <input id="cc-number2" maxlength="4">
                <input id="cc-number3" maxlength="4">
                <input id="cc-number4" maxlength="4">
                <input id="cc-exp-month" autocomplete="cc-exp-month">
                <input id="cc-exp-year" autocomplete="cc-exp-year">
                <input id="submit" type="submit">
              </form>`,
    targetElementId: "cc-type",
    formValue: {
      "cc-name": "John Doe",
      "cc-number1": "3714",
      "cc-number2": "4963",
      "cc-number3": "5398",
      "cc-number4": "431",
      "cc-exp-month": 12,
      "cc-exp-year": 2000,
      "cc-type": "amex",
    },
    expectedResult: {
      formSubmission: true,
      records: {
        address: [],
        creditCard: [
          {
            guid: null,
            record: {
              "cc-name": "John Doe",
              "cc-number": "371449635398431",
              "cc-exp-month": 12,
              "cc-exp-year": 2000,
              "cc-type": "amex",
            },
            untouchedFields: [],
          },
        ],
      },
    },
  },
  {
    description: "Trigger address and credit card saving",
    document: DEFAULT_TEST_DOC,
    targetElementId: TARGET_ELEMENT_ID,
    formValue: {
      "street-addr": "331 E. Evelyn Avenue",
      country: "US",
      tel: "1-650-903-0800",
      "cc-name": "John Doe",
      "cc-number": "5105105105105100",
      "cc-exp-month": 12,
      "cc-exp-year": 2000,
      "cc-type": "visa",
    },
    expectedResult: {
      formSubmission: true,
      records: {
        address: [
          {
            guid: null,
            record: {
              "street-address": "331 E. Evelyn Avenue",
              "address-level1": "",
              "address-level2": "",
              country: "US",
              email: "",
              tel: "1-650-903-0800",
            },
            untouchedFields: [],
          },
        ],
        creditCard: [
          {
            guid: null,
            record: {
              "cc-name": "John Doe",
              "cc-number": "5105105105105100",
              "cc-exp-month": 12,
              "cc-exp-year": 2000,
              "cc-type": "visa",
            },
            untouchedFields: [],
          },
        ],
      },
    },
  },
  {
    description: "Profile saved with trimmed string",
    document: DEFAULT_TEST_DOC,
    targetElementId: TARGET_ELEMENT_ID,
    formValue: {
      "street-addr": "331 E. Evelyn Avenue  ",
      country: "US",
      tel: "  1-650-903-0800",
    },
    expectedResult: {
      formSubmission: true,
      records: {
        address: [
          {
            guid: null,
            record: {
              "street-address": "331 E. Evelyn Avenue",
              "address-level1": "",
              "address-level2": "",
              country: "US",
              email: "",
              tel: "1-650-903-0800",
            },
            untouchedFields: [],
          },
        ],
        creditCard: [],
      },
    },
  },
  {
    description: "Eliminate the field that is empty after trimmed",
    document: DEFAULT_TEST_DOC,
    targetElementId: TARGET_ELEMENT_ID,
    formValue: {
      "street-addr": "331 E. Evelyn Avenue",
      country: "US",
      email: "  ",
      tel: "1-650-903-0800",
    },
    expectedResult: {
      formSubmission: true,
      records: {
        address: [
          {
            guid: null,
            record: {
              "street-address": "331 E. Evelyn Avenue",
              "address-level1": "",
              "address-level2": "",
              country: "US",
              email: "",
              tel: "1-650-903-0800",
            },
            untouchedFields: [],
          },
        ],
        creditCard: [],
      },
    },
  },
  {
    description: "Save state with regular select option",
    document: DEFAULT_TEST_DOC,
    targetElementId: TARGET_ELEMENT_ID,
    formValue: {
      "address-level1": "CA",
      "street-addr": "331 E. Evelyn Avenue",
      country: "US",
    },
    expectedResult: {
      formSubmission: true,
      records: {
        address: [
          {
            guid: null,
            record: {
              "address-level1": "CA",
              "address-level2": "",
              "street-address": "331 E. Evelyn Avenue",
              country: "US",
              email: "",
              tel: "",
            },
            untouchedFields: [],
          },
        ],
        creditCard: [],
      },
    },
  },
  {
    description: "Save state with lowercase value",
    document: DEFAULT_TEST_DOC,
    targetElementId: TARGET_ELEMENT_ID,
    formValue: {
      "address-level1": "ca",
      "street-addr": "331 E. Evelyn Avenue",
      country: "US",
    },
    expectedResult: {
      formSubmission: true,
      records: {
        address: [
          {
            guid: null,
            record: {
              "address-level1": "CA",
              "address-level2": "",
              "street-address": "331 E. Evelyn Avenue",
              country: "US",
              email: "",
              tel: "",
            },
            untouchedFields: [],
          },
        ],
        creditCard: [],
      },
    },
  },
  {
    description: "Save state with a country code prefixed to the label",
    document: DEFAULT_TEST_DOC,
    targetElementId: TARGET_ELEMENT_ID,
    formValue: {
      "address-level1": "AR",
      "street-addr": "331 E. Evelyn Avenue",
      country: "US",
    },
    expectedResult: {
      formSubmission: true,
      records: {
        address: [
          {
            guid: null,
            record: {
              "address-level1": "AR",
              "address-level2": "",
              "street-address": "331 E. Evelyn Avenue",
              country: "US",
              email: "",
              tel: "",
            },
            untouchedFields: [],
          },
        ],
        creditCard: [],
      },
    },
  },
  {
    description: "Save state with a country code prefixed to the value",
    document: DEFAULT_TEST_DOC,
    targetElementId: TARGET_ELEMENT_ID,
    formValue: {
      "address-level1": "US-CA",
      "street-addr": "331 E. Evelyn Avenue",
      country: "US",
    },
    expectedResult: {
      formSubmission: true,
      records: {
        address: [
          {
            guid: null,
            record: {
              "address-level1": "CA",
              "address-level2": "",
              "street-address": "331 E. Evelyn Avenue",
              country: "US",
              email: "",
              tel: "",
            },
            untouchedFields: [],
          },
        ],
        creditCard: [],
      },
    },
  },
  {
    description:
      "Save state with a country code prefixed to the value and label",
    document: DEFAULT_TEST_DOC,
    targetElementId: TARGET_ELEMENT_ID,
    formValue: {
      "address-level1": "US-AZ",
      "street-addr": "331 E. Evelyn Avenue",
      country: "US",
    },
    expectedResult: {
      formSubmission: true,
      records: {
        address: [
          {
            guid: null,
            record: {
              "address-level1": "AZ",
              "address-level2": "",
              "street-address": "331 E. Evelyn Avenue",
              country: "US",
              email: "",
              tel: "",
            },
            untouchedFields: [],
          },
        ],
        creditCard: [],
      },
    },
  },
  {
    description:
      "Should save select label instead when failed to abbreviate the value",
    document: DEFAULT_TEST_DOC,
    targetElementId: TARGET_ELEMENT_ID,
    formValue: {
      "address-level1": "Ariz",
      "street-addr": "331 E. Evelyn Avenue",
      country: "US",
    },
    expectedResult: {
      formSubmission: true,
      records: {
        address: [
          {
            guid: null,
            record: {
              "address-level1": "Arizonac",
              "address-level2": "",
              "street-address": "331 E. Evelyn Avenue",
              country: "US",
              email: "",
              tel: "",
            },
            untouchedFields: [],
          },
        ],
        creditCard: [],
      },
    },
  },
  {
    description: "Shouldn't save select with multiple selections",
    document: DEFAULT_TEST_DOC,
    targetElementId: TARGET_ELEMENT_ID,
    formValue: {
      "address-level1": ["AL", "AK", "AP"],
      "street-addr": "331 E. Evelyn Avenue",
      country: "US",
      tel: "1-650-903-0800",
    },
    expectedResult: {
      formSubmission: true,
      records: {
        address: [
          {
            guid: null,
            record: {
              "street-address": "331 E. Evelyn Avenue",
              "address-level1": "",
              "address-level2": "",
              country: "US",
              tel: "1-650-903-0800",
              email: "",
            },
            untouchedFields: [],
          },
        ],
        creditCard: [],
      },
    },
  },
  {
    description: "Shouldn't save select with empty value",
    document: DEFAULT_TEST_DOC,
    targetElementId: TARGET_ELEMENT_ID,
    formValue: {
      "address-level1": "",
      "street-addr": "331 E. Evelyn Avenue",
      country: "US",
      tel: "1-650-903-0800",
    },
    expectedResult: {
      formSubmission: true,
      records: {
        address: [
          {
            guid: null,
            record: {
              "street-address": "331 E. Evelyn Avenue",
              "address-level1": "",
              "address-level2": "",
              country: "US",
              tel: "1-650-903-0800",
              email: "",
            },
            untouchedFields: [],
          },
        ],
        creditCard: [],
      },
    },
  },
  {
    description: "Shouldn't save tel whose length is too short",
    document: DEFAULT_TEST_DOC,
    targetElementId: TARGET_ELEMENT_ID,
    formValue: {
      "street-addr": "331 E. Evelyn Avenue",
      "address-level1": "CA",
      country: "US",
      tel: "1234",
    },
    expectedResult: {
      formSubmission: true,
      records: {
        address: [
          {
            guid: null,
            record: {
              "street-address": "331 E. Evelyn Avenue",
              "address-level1": "CA",
              "address-level2": "",
              country: "US",
              tel: "",
              email: "",
            },
            untouchedFields: [],
          },
        ],
        creditCard: [],
      },
    },
  },
  {
    description: "Shouldn't save tel whose length is too long",
    document: DEFAULT_TEST_DOC,
    targetElementId: TARGET_ELEMENT_ID,
    formValue: {
      "street-addr": "331 E. Evelyn Avenue",
      "address-level1": "CA",
      country: "US",
      tel: "1234567890123456",
    },
    expectedResult: {
      formSubmission: true,
      records: {
        address: [
          {
            guid: null,
            record: {
              "street-address": "331 E. Evelyn Avenue",
              "address-level1": "CA",
              "address-level2": "",
              country: "US",
              tel: "",
              email: "",
            },
            untouchedFields: [],
          },
        ],
        creditCard: [],
      },
    },
  },
  {
    description: "Shouldn't save tel which contains invalid characters",
    document: DEFAULT_TEST_DOC,
    targetElementId: TARGET_ELEMENT_ID,
    formValue: {
      "street-addr": "331 E. Evelyn Avenue",
      "address-level1": "CA",
      country: "US",
      tel: "12345###!!",
    },
    expectedResult: {
      formSubmission: true,
      records: {
        address: [
          {
            guid: null,
            record: {
              "street-address": "331 E. Evelyn Avenue",
              "address-level1": "CA",
              "address-level2": "",
              country: "US",
              tel: "",
              email: "",
            },
            untouchedFields: [],
          },
        ],
        creditCard: [],
      },
    },
  },
];

add_task(async function handle_invalid_form() {
  info("Starting testcase: Test an invalid form element");
  let doc = MockDocument.createTestDocument(
    "http://localhost:8080/test",
    DEFAULT_TEST_DOC
  );
  let fakeForm = doc.createElement("form");
  sinon.spy(FormAutofillContent, "_onFormSubmit");

  FormAutofillContent.formSubmitted(fakeForm, null);
  Assert.equal(FormAutofillContent._onFormSubmit.called, false);
  FormAutofillContent._onFormSubmit.restore();
});

add_task(async function autofill_disabled() {
  let doc = MockDocument.createTestDocument(
    "http://localhost:8080/test",
    DEFAULT_TEST_DOC
  );
  let form = doc.getElementById("form1");
  form.reset();

  let testcase = {
    "street-addr": "331 E. Evelyn Avenue",
    country: "US",
    tel: "+16509030800",
    "cc-number": "1111222233334444",
  };
  for (let key in testcase) {
    let input = doc.getElementById(key);
    input.value = testcase[key];
  }

  let element = doc.getElementById(TARGET_ELEMENT_ID);
  FormAutofillContent.identifyAutofillFields(element);

  sinon.stub(FormAutofillContent, "_onFormSubmit");

  // "_onFormSubmit" shouldn't be called if both "addresses" and "creditCards"
  // are disabled.
  Services.prefs.setBoolPref(
    "extensions.formautofill.addresses.enabled",
    false
  );
  Services.prefs.setBoolPref(
    "extensions.formautofill.creditCards.enabled",
    false
  );
  FormAutofillContent.formSubmitted(form, null);
  Assert.equal(FormAutofillContent._onFormSubmit.called, false);
  FormAutofillContent._onFormSubmit.resetHistory();

  // "_onFormSubmit" should be called as usual.
  Services.prefs.clearUserPref("extensions.formautofill.addresses.enabled");
  Services.prefs.clearUserPref("extensions.formautofill.creditCards.enabled");

  Services.prefs.setBoolPref(
    "extensions.formautofill.creditCards.enabled",
    true
  );

  FormAutofillContent.formSubmitted(form, null);
  Assert.equal(FormAutofillContent._onFormSubmit.called, true);
  Assert.notDeepEqual(FormAutofillContent._onFormSubmit.args[0][0].address, []);
  Assert.notDeepEqual(
    FormAutofillContent._onFormSubmit.args[0][0].creditCard,
    []
  );
  FormAutofillContent._onFormSubmit.resetHistory();

  // "address" should be empty if "addresses" pref is disabled.
  Services.prefs.setBoolPref(
    "extensions.formautofill.addresses.enabled",
    false
  );
  FormAutofillContent.formSubmitted(form, null);
  Assert.equal(FormAutofillContent._onFormSubmit.called, true);
  Assert.deepEqual(FormAutofillContent._onFormSubmit.args[0][0].address, []);
  Assert.notDeepEqual(
    FormAutofillContent._onFormSubmit.args[0][0].creditCard,
    []
  );
  FormAutofillContent._onFormSubmit.resetHistory();
  Services.prefs.clearUserPref("extensions.formautofill.addresses.enabled");

  // "creditCard" should be empty if "creditCards" pref is disabled.
  Services.prefs.setBoolPref(
    "extensions.formautofill.creditCards.enabled",
    false
  );
  FormAutofillContent.formSubmitted(form, null);
  Assert.deepEqual(FormAutofillContent._onFormSubmit.called, true);
  Assert.notDeepEqual(FormAutofillContent._onFormSubmit.args[0][0].address, []);
  Assert.deepEqual(FormAutofillContent._onFormSubmit.args[0][0].creditCard, []);
  FormAutofillContent._onFormSubmit.resetHistory();
  Services.prefs.clearUserPref("extensions.formautofill.creditCards.enabled");

  FormAutofillContent._onFormSubmit.restore();
});

TESTCASES.forEach(testcase => {
  add_task(async function check_records_saving_is_called_correctly() {
    info("Starting testcase: " + testcase.description);

    Services.prefs.setBoolPref(
      "extensions.formautofill.creditCards.enabled",
      true
    );
    let doc = MockDocument.createTestDocument(
      "http://localhost:8080/test/",
      testcase.document
    );
    let form = doc.getElementById("form1");
    form.reset();
    for (let key in testcase.formValue) {
      let input = doc.getElementById(key);
      let value = testcase.formValue[key];
      if (ChromeUtils.getClassName(input) === "HTMLSelectElement" && value) {
        input.multiple = Array.isArray(value);
        [...input.options].forEach(option => {
          option.selected = value.includes(option.value);
        });
      } else {
        input.value = testcase.formValue[key];
      }
    }
    sinon.stub(FormAutofillContent, "_onFormSubmit");

    let element = doc.getElementById(testcase.targetElementId);
    FormAutofillContent.identifyAutofillFields(element);
    FormAutofillContent.formSubmitted(form, null);

    Assert.equal(
      FormAutofillContent._onFormSubmit.called,
      testcase.expectedResult.formSubmission,
      "Check expected onFormSubmit.called"
    );
    if (FormAutofillContent._onFormSubmit.called) {
      for (let ccRecord of FormAutofillContent._onFormSubmit.args[0][0]
        .creditCard) {
        delete ccRecord.flowId;
      }

      Assert.deepEqual(
        FormAutofillContent._onFormSubmit.args[0][0],
        testcase.expectedResult.records
      );
    }
    FormAutofillContent._onFormSubmit.restore();
    Services.prefs.clearUserPref("extensions.formautofill.creditCards.enabled");
  });
});
