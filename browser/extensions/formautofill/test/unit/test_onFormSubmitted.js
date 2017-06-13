"use strict";

Cu.import("resource://formautofill/FormAutofillContent.jsm");

const MOCK_DOC = MockDocument.createTestDocument("http://localhost:8080/test/",
                   `<form id="form1">
                      <input id="street-addr" autocomplete="street-address">
                      <input id="city" autocomplete="address-level2">
                      <input id="country" autocomplete="country">
                      <input id="email" autocomplete="email">
                      <input id="tel" autocomplete="tel">
                      <input id="submit" type="submit">
                    </form>`);

const TESTCASES = [
  {
    description: "Should not trigger saving if the number of fields is less than 3",
    formValue: {
      "street-addr": "331 E. Evelyn Avenue",
      "tel": "1-650-903-0800",
    },
    expectedResult: {
      formSubmission: false,
    },
  },
  {
    description: "Trigger profile saving",
    formValue: {
      "street-addr": "331 E. Evelyn Avenue",
      "country": "USA",
      "tel": "1-650-903-0800",
    },
    expectedResult: {
      formSubmission: true,
      profile: {
        address: {
          guid: null,
          record: {
            "street-address": "331 E. Evelyn Avenue",
            "country": "USA",
            "tel": "1-650-903-0800",
          },
        },
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
      profile: {
        address: {
          guid: null,
          record: {
            "street-address": "331 E. Evelyn Avenue",
            "country": "USA",
            "tel": "1-650-903-0800",
          },
        },
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
      profile: {
        address: {
          guid: null,
          record: {
            "street-address": "331 E. Evelyn Avenue",
            "country": "USA",
            "tel": "1-650-903-0800",
          },
        },
      },
    },
  },
];

add_task(async function handle_earlyformsubmit_event() {
  do_print("Starting testcase: Test an invalid form element");
  let fakeForm = MOCK_DOC.createElement("form");
  sinon.spy(FormAutofillContent, "_onFormSubmit");

  do_check_eq(FormAutofillContent.notify(fakeForm), true);
  do_check_eq(FormAutofillContent._onFormSubmit.called, false);
  FormAutofillContent._onFormSubmit.restore();
});

TESTCASES.forEach(testcase => {
  add_task(async function check_profile_saving_is_called_correctly() {
    do_print("Starting testcase: " + testcase.description);

    let form = MOCK_DOC.getElementById("form1");
    for (let key in testcase.formValue) {
      let input = MOCK_DOC.getElementById(key);
      input.value = testcase.formValue[key];
    }
    sinon.stub(FormAutofillContent, "_onFormSubmit");

    FormAutofillContent.identifyAutofillFields(MOCK_DOC);
    FormAutofillContent.notify(form);

    do_check_eq(FormAutofillContent._onFormSubmit.called,
                testcase.expectedResult.formSubmission);
    if (FormAutofillContent._onFormSubmit.called) {
      Assert.deepEqual(FormAutofillContent._onFormSubmit.args[0][0],
                       testcase.expectedResult.profile);
    }
    FormAutofillContent._onFormSubmit.restore();
  });
});
