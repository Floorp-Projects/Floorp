"use strict";

Cu.import("resource://formautofill/ProfileAutoCompleteResult.jsm");

let matchingProfiles = [{
  guid: "test-guid-1",
  organization: "Sesame Street",
  streetAddress: "123 Sesame Street.",
  tel: "1-345-345-3456.",
}, {
  guid: "test-guid-2",
  organization: "Mozilla",
  streetAddress: "331 E. Evelyn Avenue",
  tel: "1-650-903-0800",
}];

let testCases = [{
  options: {},
  matchingProfiles: matchingProfiles,
  searchString: "",
  fieldName: "",
  expected: {
    searchResult: Ci.nsIAutoCompleteResult.RESULT_SUCCESS,
    defaultIndex: 0,
    items: [{
      style: "autofill-profile",
      image: "",
    }, {
      style: "autofill-profile",
      image: "",
    }],
  },
}, {
  options: {},
  matchingProfiles: [],
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
                                               pattern.matchingProfiles,
                                               pattern.options);
    let expectedValue = pattern.expected;
    equal(actual.searchResult, expectedValue.searchResult);
    equal(actual.defaultIndex, expectedValue.defaultIndex);
    equal(actual.matchCount, expectedValue.items.length);
    expectedValue.items.forEach((item, index) => {
      // TODO: getValueAt, getLabelAt, and getCommentAt should be verified here.
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
