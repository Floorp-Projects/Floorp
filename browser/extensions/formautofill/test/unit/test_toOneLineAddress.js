"use strict";

var FormAutofillUtils;
add_setup(async () => {
  ({ FormAutofillUtils } = ChromeUtils.import(
    "resource://autofill/FormAutofillUtils.jsm"
  ));
});

add_task(async function test_getCategoriesFromFieldNames() {
  const TEST_CASES = [
    {
      strings: ["A", "B", "C", "D"],
      expectedValue: "A B C D",
    },
    {
      strings: ["A", "B", "", "D"],
      expectedValue: "A B D",
    },
    {
      strings: ["", "B", "", "D"],
      expectedValue: "B D",
    },
    {
      strings: [null, "B", " ", "D"],
      expectedValue: "B D",
    },
    {
      strings: "A B C",
      expectedValue: "A B C",
    },
    {
      strings: "A\nB\n\n\nC",
      expectedValue: "A B C",
    },
    {
      strings: "A B \nC",
      expectedValue: "A B C",
    },
    {
      strings: "A-B-C",
      expectedValue: "A B C",
      delimiter: "-",
    },
    {
      strings: "A B\n \nC",
      expectedValue: "A B C",
    },
    {
      strings: null,
      expectedValue: "",
    },
  ];

  for (let tc of TEST_CASES) {
    let result;
    if (tc.delimiter) {
      result = FormAutofillUtils.toOneLineAddress(tc.strings, tc.delimiter);
    } else {
      result = FormAutofillUtils.toOneLineAddress(tc.strings);
    }
    Assert.equal(result, tc.expectedValue);
  }
});
