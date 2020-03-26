/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests row updating and reuse.

"use strict";

let TEST_BASE_URL = "example.com/";
if (UrlbarPrefs.get("update1.view.stripHttps")) {
  TEST_BASE_URL = "http://" + TEST_BASE_URL;
}

add_task(async function init() {
  await PlacesUtils.history.clear();
  await PlacesUtils.bookmarks.eraseEverything();
});

// A URL result is replaced with a tip result and then vice versa.
add_task(async function urlToTip() {
  // Add some visits that will be matched by a "test" search string.
  await PlacesTestUtils.addVisits([
    "http://example.com/testxx",
    "http://example.com/test",
  ]);

  // Add a provider that returns a tip result when the search string is "testx".
  let provider = new UrlbarTestUtils.TestProvider({
    results: [
      new UrlbarResult(
        UrlbarUtils.RESULT_TYPE.TIP,
        UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        {
          text: "This is a test tip.",
          buttonText: "OK",
          helpUrl: "http://example.com/",
          type: "test",
        }
      ),
    ],
  });
  provider.isActive = context => context.searchString == "testx";
  UrlbarProvidersManager.registerProvider(provider);

  // Search for "test".
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    value: "test",
    window,
    waitForFocus,
  });

  // The result at index 1 should be the http://example.com/test visit.
  await checkResult(
    1,
    UrlbarUtils.RESULT_TYPE.URL,
    {
      title: "test visit for http://example.com/test",
      tagsContainer: null,
      titleSeparator: null,
      action: "",
      url: TEST_BASE_URL + "test",
    },
    ["tipButton", "helpButton"]
  );

  // Type an "x" so that the search string is "testx".
  EventUtils.synthesizeKey("x");
  await UrlbarTestUtils.promiseSearchComplete(window);

  // Now the result at index 1 should be the tip from our provider.
  await checkResult(
    1,
    UrlbarUtils.RESULT_TYPE.TIP,
    {
      title: "This is a test tip.",
      tipButton: "OK",
      helpButton: null,
    },
    ["tagsContainer", "titleSeparator", "action", "url"]
  );

  // Type another "x" so that the search string is "testxx".
  EventUtils.synthesizeKey("x");
  await UrlbarTestUtils.promiseSearchComplete(window);

  // The result at index 1 should be the http://example.com/testxx visit.
  await checkResult(
    1,
    UrlbarUtils.RESULT_TYPE.URL,
    {
      title: "test visit for http://example.com/testxx",
      tagsContainer: null,
      titleSeparator: null,
      action: "",
      url: TEST_BASE_URL + "testxx",
    },
    ["tipButton", "helpButton"]
  );

  // Backspace so that the search string is "testx" again.
  EventUtils.synthesizeKey("KEY_Backspace");
  await UrlbarTestUtils.promiseSearchComplete(window);

  // The result at index 1 should be the tip again.
  await checkResult(
    1,
    UrlbarUtils.RESULT_TYPE.TIP,
    {
      title: "This is a test tip.",
      tipButton: "OK",
      helpButton: null,
    },
    ["tagsContainer", "titleSeparator", "action", "url"]
  );

  await UrlbarTestUtils.promisePopupClose(window, () => {
    EventUtils.synthesizeKey("KEY_Escape");
  });
  UrlbarProvidersManager.unregisterProvider(provider);
  await PlacesUtils.history.clear();
});

// A tip result is replaced with URL result and then vice versa.
add_task(async function tipToURL() {
  // Add a visit that will be matched by a "testx" search string.
  await PlacesTestUtils.addVisits("http://example.com/testx");

  // Add a provider that returns a tip result when the search string is "test"
  // or "testxx".
  let provider = new UrlbarTestUtils.TestProvider({
    results: [
      new UrlbarResult(
        UrlbarUtils.RESULT_TYPE.TIP,
        UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        {
          text: "This is a test tip.",
          buttonText: "OK",
          helpUrl: "http://example.com/",
          type: "test",
        }
      ),
    ],
  });
  provider.isActive = context =>
    ["test", "testxx"].includes(context.searchString);
  UrlbarProvidersManager.registerProvider(provider);

  // Search for "test".
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    value: "test",
    window,
    waitForFocus,
  });

  // The result at index 1 should be the tip from our provider.
  await checkResult(
    1,
    UrlbarUtils.RESULT_TYPE.TIP,
    {
      title: "This is a test tip.",
      tipButton: "OK",
      helpButton: null,
    },
    ["tagsContainer", "titleSeparator", "action", "url"]
  );

  // Type an "x" so that the search string is "testx".
  EventUtils.synthesizeKey("x");
  await UrlbarTestUtils.promiseSearchComplete(window);

  // Now the result at index 1 should be the visit.
  await checkResult(
    1,
    UrlbarUtils.RESULT_TYPE.URL,
    {
      title: "test visit for http://example.com/testx",
      tagsContainer: null,
      titleSeparator: null,
      action: "",
      url: TEST_BASE_URL + "testx",
    },
    ["tipButton", "helpButton"]
  );

  // Type another "x" so that the search string is "testxx".
  EventUtils.synthesizeKey("x");
  await UrlbarTestUtils.promiseSearchComplete(window);

  // The result at index 1 should be the tip again.
  await checkResult(
    1,
    UrlbarUtils.RESULT_TYPE.TIP,
    {
      title: "This is a test tip.",
      tipButton: "OK",
      helpButton: null,
    },
    ["tagsContainer", "titleSeparator", "action", "url"]
  );

  // Backspace so that the search string is "testx" again.
  EventUtils.synthesizeKey("KEY_Backspace");
  await UrlbarTestUtils.promiseSearchComplete(window);

  // The result at index 1 should be the visit again.
  await checkResult(
    1,
    UrlbarUtils.RESULT_TYPE.URL,
    {
      title: "test visit for http://example.com/testx",
      tagsContainer: null,
      titleSeparator: null,
      action: "",
      url: TEST_BASE_URL + "testx",
    },
    ["tipButton", "helpButton"]
  );

  await UrlbarTestUtils.promisePopupClose(window, () => {
    EventUtils.synthesizeKey("KEY_Escape");
  });
  UrlbarProvidersManager.unregisterProvider(provider);
  await PlacesUtils.history.clear();
});

async function checkResult(index, type, presentElements, absentElements) {
  let result = await UrlbarTestUtils.getDetailsOfResultAt(window, index);
  Assert.equal(result.type, type, "Expected result type");

  for (let [name, value] of Object.entries(presentElements)) {
    let element = result.element.row._elements.get(name);
    Assert.ok(element, `${name} should be present`);
    if (typeof value == "string") {
      Assert.equal(
        element.textContent,
        value,
        `${name} value should be correct`
      );
    }
  }

  for (let name of absentElements) {
    let element = result.element.row._elements.get(name);
    Assert.ok(!element, `${name} should be absent`);
  }
}
