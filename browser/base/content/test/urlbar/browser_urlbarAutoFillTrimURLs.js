// This test ensures that autoFilled values are not trimmed, unless the user
// selects from the autocomplete popup.

add_task(async function setup() {
  const PREF_TRIMURL = "browser.urlbar.trimURLs";
  const PREF_AUTOFILL = "browser.urlbar.autoFill";

  registerCleanupFunction(async function() {
    Services.prefs.clearUserPref(PREF_TRIMURL);
    Services.prefs.clearUserPref(PREF_AUTOFILL);
    await PlacesUtils.history.clear();
    gURLBar.handleRevert();
  });
  Services.prefs.setBoolPref(PREF_TRIMURL, true);
  Services.prefs.setBoolPref(PREF_AUTOFILL, true);

  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesUtils.history.clear();

  // Adding a tab would hit switch-to-tab, so it's safer to just add a visit.
  await PlacesTestUtils.addVisits([{
    uri: "http://www.autofilltrimurl.com/whatever",
  }, {
    uri: "https://www.secureautofillurl.com/whatever",
  }]);
});

async function promiseSearch(searchtext) {
  gURLBar.focus();
  gURLBar.inputField.value = searchtext.substr(0, searchtext.length - 1);
  EventUtils.sendString(searchtext.substr(-1, 1));
  await promiseSearchComplete();
}

async function promiseTestResult(test) {
  info("Searching for '${test.search}'");

  await promiseSearch(test.search);

  is(gURLBar.inputField.value, test.autofilledValue,
     `Autofilled value is as expected for search '${test.search}'`);

  let result = gURLBar.popup.richlistbox.getItemAtIndex(0);

  is(result._titleText.textContent, test.resultListDisplayTitle,
     `Autocomplete result should have displayed title as expected for search '${test.search}'`);

  is(result._actionText.textContent, test.resultListActionText,
     `Autocomplete action text should be as expected for search '${test.search}'`);

  is(result.getAttribute("type"), test.resultListType,
     `Autocomplete result should have searchengine for the type for search '${test.search}'`);

  is(gURLBar.mController.getFinalCompleteValueAt(0), test.finalCompleteValue,
     `Autocomplete item should go to the expected final value for search '${test.search}'`);
}

const tests = [{
    search: "http://",
    autofilledValue: "http://",
    resultListDisplayTitle: "http://",
    resultListActionText: "Search with Google",
    resultListType: "searchengine",
    finalCompleteValue: 'moz-action:searchengine,{"engineName":"Google","input":"http%3A%2F%2F","searchQuery":"http%3A%2F%2F"}'
  },
  {
    search: "https://",
    autofilledValue: "https://",
    resultListDisplayTitle: "https://",
    resultListActionText: "Search with Google",
    resultListType: "searchengine",
    finalCompleteValue: 'moz-action:searchengine,{"engineName":"Google","input":"https%3A%2F%2F","searchQuery":"https%3A%2F%2F"}'
  },
  {
    search: "au",
    autofilledValue: "autofilltrimurl.com/",
    resultListDisplayTitle: "www.autofilltrimurl.com",
    resultListActionText: "Visit",
    resultListType: "",
    finalCompleteValue: "http://www.autofilltrimurl.com/"
  },
  {
    search: "http://au",
    autofilledValue: "http://autofilltrimurl.com/",
    resultListDisplayTitle: "www.autofilltrimurl.com",
    resultListActionText: "Visit",
    resultListType: "",
    finalCompleteValue: "http://www.autofilltrimurl.com/"
  },
  {
    search: "sec",
    autofilledValue: "secureautofillurl.com/",
    resultListDisplayTitle: "https://www.secureautofillurl.com",
    resultListActionText: "Visit",
    resultListType: "",
    finalCompleteValue: "https://www.secureautofillurl.com/"
  },
  {
    search: "https://sec",
    autofilledValue: "https://secureautofillurl.com/",
    resultListDisplayTitle: "https://www.secureautofillurl.com",
    resultListActionText: "Visit",
    resultListType: "",
    finalCompleteValue: "https://www.secureautofillurl.com/"
  },
];

add_task(async function autofill_tests() {
  for (let test of tests) {
    await promiseTestResult(test);
  }
});

add_task(async function autofill_complete_domain() {
  await promiseSearch("http://www.autofilltrimurl.com");
  is(gURLBar.inputField.value, "http://www.autofilltrimurl.com/", "Autofilled value is as expected");

  // Now ensure selecting from the popup correctly trims.
  is(gURLBar.controller.matchCount, 2, "Found the expected number of matches");
  EventUtils.synthesizeKey("KEY_ArrowDown");
  is(gURLBar.inputField.value, "www.autofilltrimurl.com/whatever", "trim was applied correctly");
});
