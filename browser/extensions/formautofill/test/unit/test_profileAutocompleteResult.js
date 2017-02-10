"use strict";

Cu.import("resource://formautofill/ProfileAutoCompleteResult.jsm");

let matchingProfiles = [{
  guid: "test-guid-1",
  organization: "Sesame Street",
  "street-address": "123 Sesame Street.",
  tel: "1-345-345-3456.",
}, {
  guid: "test-guid-2",
  organization: "Mozilla",
  "street-address": "331 E. Evelyn Avenue",
  tel: "1-650-903-0800",
}];

let allFieldNames = ["street-address", "organization", "tel"];

let testCases = [{
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
    }],
  },
}, {
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
        secondary: "Sesame Street",
      }),
      image: "",
    }, {
      value: "331 E. Evelyn Avenue",
      style: "autofill-profile",
      comment: JSON.stringify(matchingProfiles[1]),
      label: JSON.stringify({
        primary: "331 E. Evelyn Avenue",
        secondary: "Mozilla",
      }),
      image: "",
    }],
  },
}, {
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
  testCases.forEach(pattern => {
    let actual = new ProfileAutoCompleteResult(pattern.searchString,
                                               pattern.fieldName,
                                               pattern.allFieldNames,
                                               pattern.matchingProfiles,
                                               pattern.options);
    let expectedValue = pattern.expected;
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
