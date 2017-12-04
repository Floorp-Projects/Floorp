"use strict";

Cu.import("resource://formautofill/FormAutofillContent.jsm");

const MOCK_DOC = MockDocument.createTestDocument("http://localhost:8080/test/",
                   `<form id="form1">
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
                      <input id="submit" type="submit">
                    </form>`);
const TARGET_ELEMENT_ID = "street-addr";

const TESTCASES = [
  {
    description: "Should not trigger address saving if the number of fields is less than 3",
    formValue: {
      "street-addr": "331 E. Evelyn Avenue",
      "tel": "1-650-903-0800",
    },
    expectedResult: {
      formSubmission: false,
    },
  },
  {
    description: "Should not trigger credit card saving if number is empty",
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
    description: "Trigger address saving",
    formValue: {
      "street-addr": "331 E. Evelyn Avenue",
      "country": "USA",
      "tel": "1-650-903-0800",
    },
    expectedResult: {
      formSubmission: true,
      records: {
        address: [{
          guid: null,
          record: {
            "street-address": "331 E. Evelyn Avenue",
            "address-level1": "",
            "address-level2": "",
            "country": "USA",
            "email": "",
            "tel": "1-650-903-0800",
          },
          untouchedFields: [],
        }],
        creditCard: [],
      },
    },
  },
  {
    description: "Trigger credit card saving",
    formValue: {
      "cc-name": "John Doe",
      "cc-number": "1234567812345678",
      "cc-exp-month": 12,
      "cc-exp-year": 2000,
    },
    expectedResult: {
      formSubmission: true,
      records: {
        address: [],
        creditCard: [{
          guid: null,
          record: {
            "cc-name": "John Doe",
            "cc-number": "1234567812345678",
            "cc-exp-month": 12,
            "cc-exp-year": 2000,
          },
          untouchedFields: [],
        }],
      },
    },
  },
  {
    description: "Trigger address and credit card saving",
    formValue: {
      "street-addr": "331 E. Evelyn Avenue",
      "country": "USA",
      "tel": "1-650-903-0800",
      "cc-name": "John Doe",
      "cc-number": "1234567812345678",
      "cc-exp-month": 12,
      "cc-exp-year": 2000,
    },
    expectedResult: {
      formSubmission: true,
      records: {
        address: [{
          guid: null,
          record: {
            "street-address": "331 E. Evelyn Avenue",
            "address-level1": "",
            "address-level2": "",
            "country": "USA",
            "email": "",
            "tel": "1-650-903-0800",
          },
          untouchedFields: [],
        }],
        creditCard: [{
          guid: null,
          record: {
            "cc-name": "John Doe",
            "cc-number": "1234567812345678",
            "cc-exp-month": 12,
            "cc-exp-year": 2000,
          },
          untouchedFields: [],
        }],
      },
    },
  },
  {
    description: "Profile saved with trimmed string",
    formValue: {
      "street-addr": "331 E. Evelyn Avenue  ",
      "country": "USA",
      "tel": "  1-650-903-0800",
    },
    expectedResult: {
      formSubmission: true,
      records: {
        address: [{
          guid: null,
          record: {
            "street-address": "331 E. Evelyn Avenue",
            "address-level1": "",
            "address-level2": "",
            "country": "USA",
            "email": "",
            "tel": "1-650-903-0800",
          },
          untouchedFields: [],
        }],
        creditCard: [],
      },
    },
  },
  {
    description: "Eliminate the field that is empty after trimmed",
    formValue: {
      "street-addr": "331 E. Evelyn Avenue",
      "country": "USA",
      "email": "  ",
      "tel": "1-650-903-0800",
    },
    expectedResult: {
      formSubmission: true,
      records: {
        address: [{
          guid: null,
          record: {
            "street-address": "331 E. Evelyn Avenue",
            "address-level1": "",
            "address-level2": "",
            "country": "USA",
            "email": "",
            "tel": "1-650-903-0800",
          },
          untouchedFields: [],
        }],
        creditCard: [],
      },
    },
  },
  {
    description: "Save state with regular select option",
    formValue: {
      "address-level1": "CA",
      "street-addr": "331 E. Evelyn Avenue",
      "country": "USA",
    },
    expectedResult: {
      formSubmission: true,
      records: {
        address: [{
          guid: null,
          record: {
            "address-level1": "CA",
            "address-level2": "",
            "street-address": "331 E. Evelyn Avenue",
            "country": "USA",
            "email": "",
            "tel": "",
          },
          untouchedFields: [],
        }],
        creditCard: [],
      },
    },
  },
  {
    description: "Save state with lowercase value",
    formValue: {
      "address-level1": "ca",
      "street-addr": "331 E. Evelyn Avenue",
      "country": "USA",
    },
    expectedResult: {
      formSubmission: true,
      records: {
        address: [{
          guid: null,
          record: {
            "address-level1": "CA",
            "address-level2": "",
            "street-address": "331 E. Evelyn Avenue",
            "country": "USA",
            "email": "",
            "tel": "",
          },
          untouchedFields: [],
        }],
        creditCard: [],
      },
    },
  },
  {
    description: "Save state with a country code prefixed to the label",
    formValue: {
      "address-level1": "AR",
      "street-addr": "331 E. Evelyn Avenue",
      "country": "USA",
    },
    expectedResult: {
      formSubmission: true,
      records: {
        address: [{
          guid: null,
          record: {
            "address-level1": "AR",
            "address-level2": "",
            "street-address": "331 E. Evelyn Avenue",
            "country": "USA",
            "email": "",
            "tel": "",
          },
          untouchedFields: [],
        }],
        creditCard: [],
      },
    },
  },
  {
    description: "Save state with a country code prefixed to the value",
    formValue: {
      "address-level1": "US-CA",
      "street-addr": "331 E. Evelyn Avenue",
      "country": "USA",
    },
    expectedResult: {
      formSubmission: true,
      records: {
        address: [{
          guid: null,
          record: {
            "address-level1": "CA",
            "address-level2": "",
            "street-address": "331 E. Evelyn Avenue",
            "country": "USA",
            "email": "",
            "tel": "",
          },
          untouchedFields: [],
        }],
        creditCard: [],
      },
    },
  },
  {
    description: "Save state with a country code prefixed to the value and label",
    formValue: {
      "address-level1": "US-AZ",
      "street-addr": "331 E. Evelyn Avenue",
      "country": "USA",
    },
    expectedResult: {
      formSubmission: true,
      records: {
        address: [{
          guid: null,
          record: {
            "address-level1": "AZ",
            "address-level2": "",
            "street-address": "331 E. Evelyn Avenue",
            "country": "USA",
            "email": "",
            "tel": "",
          },
          untouchedFields: [],
        }],
        creditCard: [],
      },
    },
  },
  {
    description: "Should save select label instead when failed to abbreviate the value",
    formValue: {
      "address-level1": "Ariz",
      "street-addr": "331 E. Evelyn Avenue",
      "country": "USA",
    },
    expectedResult: {
      formSubmission: true,
      records: {
        address: [{
          guid: null,
          record: {
            "address-level1": "Arizonac",
            "address-level2": "",
            "street-address": "331 E. Evelyn Avenue",
            "country": "USA",
            "email": "",
            "tel": "",
          },
          untouchedFields: [],
        }],
        creditCard: [],
      },
    },
  },
  {
    description: "Shouldn't save select with multiple selections",
    formValue: {
      "address-level1": ["AL", "AK", "AP"],
      "street-addr": "331 E. Evelyn Avenue",
      "country": "USA",
      "tel": "1-650-903-0800",
    },
    expectedResult: {
      formSubmission: true,
      records: {
        address: [{
          guid: null,
          record: {
            "street-address": "331 E. Evelyn Avenue",
            "address-level1": "",
            "address-level2": "",
            "country": "USA",
            "tel": "1-650-903-0800",
            "email": "",
          },
          untouchedFields: [],
        }],
        creditCard: [],
      },
    },
  },
  {
    description: "Shouldn't save select with empty value",
    formValue: {
      "address-level1": "",
      "street-addr": "331 E. Evelyn Avenue",
      "country": "USA",
      "tel": "1-650-903-0800",
    },
    expectedResult: {
      formSubmission: true,
      records: {
        address: [{
          guid: null,
          record: {
            "street-address": "331 E. Evelyn Avenue",
            "address-level1": "",
            "address-level2": "",
            "country": "USA",
            "tel": "1-650-903-0800",
            "email": "",
          },
          untouchedFields: [],
        }],
        creditCard: [],
      },
    },
  },
  {
    description: "Shouldn't save tel whose length is too short",
    formValue: {
      "street-addr": "331 E. Evelyn Avenue",
      "address-level1": "CA",
      "country": "US",
      "tel": "1234",
    },
    expectedResult: {
      formSubmission: true,
      records: {
        address: [{
          guid: null,
          record: {
            "street-address": "331 E. Evelyn Avenue",
            "address-level1": "CA",
            "address-level2": "",
            "country": "US",
            "tel": "",
            "email": "",
          },
          untouchedFields: [],
        }],
        creditCard: [],
      },
    },
  },
  {
    description: "Shouldn't save tel whose length is too long",
    formValue: {
      "street-addr": "331 E. Evelyn Avenue",
      "address-level1": "CA",
      "country": "US",
      "tel": "1234567890123456",
    },
    expectedResult: {
      formSubmission: true,
      records: {
        address: [{
          guid: null,
          record: {
            "street-address": "331 E. Evelyn Avenue",
            "address-level1": "CA",
            "address-level2": "",
            "country": "US",
            "tel": "",
            "email": "",
          },
          untouchedFields: [],
        }],
        creditCard: [],
      },
    },
  },
  {
    description: "Shouldn't save tel which contains invalid characters",
    formValue: {
      "street-addr": "331 E. Evelyn Avenue",
      "address-level1": "CA",
      "country": "US",
      "tel": "12345###!!",
    },
    expectedResult: {
      formSubmission: true,
      records: {
        address: [{
          guid: null,
          record: {
            "street-address": "331 E. Evelyn Avenue",
            "address-level1": "CA",
            "address-level2": "",
            "country": "US",
            "tel": "",
            "email": "",
          },
          untouchedFields: [],
        }],
        creditCard: [],
      },
    },
  },
];

add_task(async function handle_earlyformsubmit_event() {
  do_print("Starting testcase: Test an invalid form element");
  let fakeForm = MOCK_DOC.createElement("form");
  sinon.spy(FormAutofillContent, "_onFormSubmit");

  Assert.equal(FormAutofillContent.notify(fakeForm), true);
  Assert.equal(FormAutofillContent._onFormSubmit.called, false);
  FormAutofillContent._onFormSubmit.restore();
});

add_task(async function autofill_disabled() {
  let form = MOCK_DOC.getElementById("form1");
  form.reset();

  let testcase = {
    "street-addr": "331 E. Evelyn Avenue",
    "country": "US",
    "tel": "+16509030800",
    "cc-number": "1111222233334444",
  };
  for (let key in testcase) {
    let input = MOCK_DOC.getElementById(key);
    input.value = testcase[key];
  }

  let element = MOCK_DOC.getElementById(TARGET_ELEMENT_ID);
  FormAutofillContent.identifyAutofillFields(element);

  sinon.stub(FormAutofillContent, "_onFormSubmit");

  // "_onFormSubmit" shouldn't be called if both "addresses" and "creditCards"
  // are disabled.
  Services.prefs.setBoolPref("extensions.formautofill.addresses.enabled", false);
  Services.prefs.setBoolPref("extensions.formautofill.creditCards.enabled", false);
  FormAutofillContent.notify(form);
  Assert.equal(FormAutofillContent._onFormSubmit.called, false);
  FormAutofillContent._onFormSubmit.reset();

  // "_onFormSubmit" should be called as usual.
  Services.prefs.clearUserPref("extensions.formautofill.addresses.enabled");
  Services.prefs.clearUserPref("extensions.formautofill.creditCards.enabled");
  FormAutofillContent.notify(form);
  Assert.equal(FormAutofillContent._onFormSubmit.called, true);
  Assert.notDeepEqual(FormAutofillContent._onFormSubmit.args[0][0].address, []);
  Assert.notDeepEqual(FormAutofillContent._onFormSubmit.args[0][0].creditCard, []);
  FormAutofillContent._onFormSubmit.reset();

  // "address" should be empty if "addresses" pref is disabled.
  Services.prefs.setBoolPref("extensions.formautofill.addresses.enabled", false);
  FormAutofillContent.notify(form);
  Assert.equal(FormAutofillContent._onFormSubmit.called, true);
  Assert.deepEqual(FormAutofillContent._onFormSubmit.args[0][0].address, []);
  Assert.notDeepEqual(FormAutofillContent._onFormSubmit.args[0][0].creditCard, []);
  FormAutofillContent._onFormSubmit.reset();
  Services.prefs.clearUserPref("extensions.formautofill.addresses.enabled");

  // "creditCard" should be empty if "creditCards" pref is disabled.
  Services.prefs.setBoolPref("extensions.formautofill.creditCards.enabled", false);
  FormAutofillContent.notify(form);
  Assert.deepEqual(FormAutofillContent._onFormSubmit.called, true);
  Assert.notDeepEqual(FormAutofillContent._onFormSubmit.args[0][0].address, []);
  Assert.deepEqual(FormAutofillContent._onFormSubmit.args[0][0].creditCard, []);
  FormAutofillContent._onFormSubmit.reset();
  Services.prefs.clearUserPref("extensions.formautofill.creditCards.enabled");

  FormAutofillContent._onFormSubmit.restore();
});

TESTCASES.forEach(testcase => {
  add_task(async function check_records_saving_is_called_correctly() {
    do_print("Starting testcase: " + testcase.description);

    let form = MOCK_DOC.getElementById("form1");
    form.reset();
    for (let key in testcase.formValue) {
      let input = MOCK_DOC.getElementById(key);
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

    let element = MOCK_DOC.getElementById(TARGET_ELEMENT_ID);
    FormAutofillContent.identifyAutofillFields(element);
    FormAutofillContent.notify(form);

    Assert.equal(FormAutofillContent._onFormSubmit.called,
                testcase.expectedResult.formSubmission);
    if (FormAutofillContent._onFormSubmit.called) {
      Assert.deepEqual(FormAutofillContent._onFormSubmit.args[0][0],
                       testcase.expectedResult.records);
    }
    FormAutofillContent._onFormSubmit.restore();
  });
});
