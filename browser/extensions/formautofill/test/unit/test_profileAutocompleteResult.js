"use strict";

Cu.import("resource://formautofill/ProfileAutoCompleteResult.jsm");

let matchingProfiles = [{
  guid: "test-guid-1",
  "given-name": "Timothy",
  "family-name": "Berners-Lee",
  name: "Timothy Berners-Lee",
  organization: "Sesame Street",
  "street-address": "123 Sesame Street.",
  tel: "1-345-345-3456.",
}, {
  guid: "test-guid-2",
  "given-name": "John",
  "family-name": "Doe",
  name: "John Doe",
  organization: "Mozilla",
  "street-address": "331 E. Evelyn Avenue",
  tel: "1-650-903-0800",
}, {
  guid: "test-guid-3",
  organization: "",
  "street-address": "321, No Name St.",
  tel: "1-000-000-0000",
}];

let allFieldNames = ["given-name", "family-name", "street-address", "organization", "tel"];

let testCases = [{
  description: "Focus on an `organization` field",
  options: {},
  matchingProfiles,
  allFieldNames,
  searchString: "",
  fieldName: "organization",
  expected: {
    searchResult: Ci.nsIAutoCompleteResult.RESULT_SUCCESS,
    defaultIndex: 0,
    items: [{
      value: "Sesame Street",
      style: "autofill-profile",
      comment: JSON.stringify(matchingProfiles[0]),
      label: JSON.stringify({
        primary: "Sesame Street",
        secondary: "123 Sesame Street.",
      }),
      image: "",
    }, {
      value: "Mozilla",
      style: "autofill-profile",
      comment: JSON.stringify(matchingProfiles[1]),
      label: JSON.stringify({
        primary: "Mozilla",
        secondary: "331 E. Evelyn Avenue",
      }),
      image: "",
    }],
  },
}, {
  description: "Focus on an `tel` field",
  options: {},
  matchingProfiles,
  allFieldNames,
  searchString: "",
  fieldName: "tel",
  expected: {
    searchResult: Ci.nsIAutoCompleteResult.RESULT_SUCCESS,
    defaultIndex: 0,
    items: [{
      value: "1-345-345-3456.",
      style: "autofill-profile",
      comment: JSON.stringify(matchingProfiles[0]),
      label: JSON.stringify({
        primary: "1-345-345-3456.",
        secondary: "123 Sesame Street.",
      }),
      image: "",
    }, {
      value: "1-650-903-0800",
      style: "autofill-profile",
      comment: JSON.stringify(matchingProfiles[1]),
      label: JSON.stringify({
        primary: "1-650-903-0800",
        secondary: "331 E. Evelyn Avenue",
      }),
      image: "",
    }, {
      value: "1-000-000-0000",
      style: "autofill-profile",
      comment: JSON.stringify(matchingProfiles[2]),
      label: JSON.stringify({
        primary: "1-000-000-0000",
        secondary: "321, No Name St.",
      }),
      image: "",
    }],
  },
}, {
  description: "Focus on an `street-address` field",
  options: {},
  matchingProfiles,
  allFieldNames,
  searchString: "",
  fieldName: "street-address",
  expected: {
    searchResult: Ci.nsIAutoCompleteResult.RESULT_SUCCESS,
    defaultIndex: 0,
    items: [{
      value: "123 Sesame Street.",
      style: "autofill-profile",
      comment: JSON.stringify(matchingProfiles[0]),
      label: JSON.stringify({
        primary: "123 Sesame Street.",
        secondary: "Timothy Berners-Lee",
      }),
      image: "",
    }, {
      value: "331 E. Evelyn Avenue",
      style: "autofill-profile",
      comment: JSON.stringify(matchingProfiles[1]),
      label: JSON.stringify({
        primary: "331 E. Evelyn Avenue",
        secondary: "John Doe",
      }),
      image: "",
    }, {
      value: "321, No Name St.",
      style: "autofill-profile",
      comment: JSON.stringify(matchingProfiles[2]),
      label: JSON.stringify({
        primary: "321, No Name St.",
        secondary: "1-000-000-0000",
      }),
      image: "",
    }],
  },
}, {
  description: "No matching profiles",
  options: {},
  matchingProfiles: [],
  allFieldNames,
  searchString: "",
  fieldName: "",
  expected: {
    searchResult: Ci.nsIAutoCompleteResult.RESULT_NOMATCH,
    defaultIndex: 0,
    items: [],
  },
}, {
  description: "Search with failure",
  options: {resultCode: Ci.nsIAutoCompleteResult.RESULT_FAILURE},
  matchingProfiles: [],
  allFieldNames,
  searchString: "",
  fieldName: "",
  expected: {
    searchResult: Ci.nsIAutoCompleteResult.RESULT_FAILURE,
    defaultIndex: 0,
    items: [],
  },
}];

add_task(function* test_all_patterns() {
  testCases.forEach(testCase => {
    do_print("Starting testcase: " + testCase.description);
    let actual = new ProfileAutoCompleteResult(testCase.searchString,
                                               testCase.fieldName,
                                               testCase.allFieldNames,
                                               testCase.matchingProfiles,
                                               testCase.options);
    let expectedValue = testCase.expected;
    equal(actual.searchResult, expectedValue.searchResult);
    equal(actual.defaultIndex, expectedValue.defaultIndex);
    equal(actual.matchCount, expectedValue.items.length);
    expectedValue.items.forEach((item, index) => {
      equal(actual.getValueAt(index), item.value);
      equal(actual.getCommentAt(index), item.comment);
      equal(actual.getLabelAt(index), item.label);
      equal(actual.getStyleAt(index), item.style);
      equal(actual.getImageAt(index), item.image);
    });

    if (expectedValue.items.length != 0) {
      Assert.throws(() => actual.getValueAt(expectedValue.items.length),
        /Index out of range\./);

      Assert.throws(() => actual.getLabelAt(expectedValue.items.length),
        /Index out of range\./);

      Assert.throws(() => actual.getCommentAt(expectedValue.items.length),
        /Index out of range\./);
    }
  });
});
