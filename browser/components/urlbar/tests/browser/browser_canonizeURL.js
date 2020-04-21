/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests turning non-url-looking values typed in the input field into proper URLs.
 */

const TEST_ENGINE_BASENAME = "searchSuggestionEngine.xml";

add_task(async function checkCtrlWorks() {
  let defaultEngine = await Services.search.getDefault();
  let testcases = [
    ["example", "http://www.example.com/", { ctrlKey: true }],
    // Check that a direct load is not overwritten by a previous canonization.
    ["http://example.com/test/", "http://example.com/test/", {}],
    ["ex-ample", "http://www.ex-ample.com/", { ctrlKey: true }],
    ["  example ", "http://www.example.com/", { ctrlKey: true }],
    [" example/foo ", "http://www.example.com/foo", { ctrlKey: true }],
    [
      " example/foo bar ",
      "http://www.example.com/foo%20bar",
      { ctrlKey: true },
    ],
    ["example.net", "http://example.net/", { ctrlKey: true }],
    ["http://example", "http://example/", { ctrlKey: true }],
    ["example:8080", "http://example:8080/", { ctrlKey: true }],
    ["ex-ample.foo", "http://ex-ample.foo/", { ctrlKey: true }],
    ["example.foo/bar ", "http://example.foo/bar", { ctrlKey: true }],
    ["1.1.1.1", "http://1.1.1.1/", { ctrlKey: true }],
    ["ftp.example.bar", "http://ftp.example.bar/", { ctrlKey: true }],
    [
      "ex ample",
      defaultEngine.getSubmission("ex ample", null, "keyword").uri.spec,
      { ctrlKey: true },
    ],
  ];

  if (Services.prefs.getBoolPref("network.ftp.enabled")) {
    // Include FTP testcase only if FTP protocol handler is enabled, otherwise
    // the test would hang on external application chooser popup.
    testcases.push(["ftp://example", "ftp://example/", { ctrlKey: true }]);
  }

  // Disable autoFill for this test, since it could mess up the results.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.autoFill", false],
      ["browser.urlbar.ctrlCanonizesURLs", true],
    ],
  });

  for (let [inputValue, expectedURL, options] of testcases) {
    info(`Testing input string: "${inputValue}" - expected: "${expectedURL}"`);
    let promiseLoad = BrowserTestUtils.waitForDocLoadAndStopIt(
      expectedURL,
      gBrowser.selectedBrowser
    );
    gURLBar.focus();
    gURLBar.inputField.value = inputValue.slice(0, -1);
    EventUtils.sendString(inputValue.slice(-1));
    EventUtils.synthesizeKey("KEY_Enter", options);
    await promiseLoad;
  }
});

add_task(async function checkPrefTurnsOffCanonize() {
  // Add a dummy search engine to avoid hitting the network.
  let engine = await SearchTestUtils.promiseNewSearchEngine(
    getRootDirectory(gTestPath) + TEST_ENGINE_BASENAME
  );
  let oldDefaultEngine = await Services.search.getDefault();
  await Services.search.setDefault(engine);
  registerCleanupFunction(async () =>
    Services.search.setDefault(oldDefaultEngine)
  );

  // Ensure we don't end up loading something in the current tab becuase it's empty:
  let initialTab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: "about:mozilla",
  });
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.ctrlCanonizesURLs", false]],
  });

  let newURL = "http://mochi.test:8888/?terms=example";
  // On MacOS CTRL+Enter is not supposed to open in a new tab, because it uses
  // CMD+Enter for that.
  let promiseLoaded =
    AppConstants.platform == "macosx"
      ? BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser, false, newURL)
      : BrowserTestUtils.waitForNewTab(gBrowser);

  gURLBar.focus();
  gURLBar.selectionStart = gURLBar.selectionEnd =
    gURLBar.inputField.value.length;
  gURLBar.inputField.value = "exampl";
  EventUtils.sendString("e");
  EventUtils.synthesizeKey("KEY_Enter", { ctrlKey: true });

  await promiseLoaded;
  if (AppConstants.platform == "macosx") {
    Assert.equal(
      initialTab.linkedBrowser.currentURI.spec,
      newURL,
      "Original tab should have navigated"
    );
  } else {
    Assert.equal(
      initialTab.linkedBrowser.currentURI.spec,
      "about:mozilla",
      "Original tab shouldn't have navigated"
    );
    Assert.equal(
      gBrowser.selectedBrowser.currentURI.spec,
      newURL,
      "New tab should have navigated"
    );
  }
  while (gBrowser.tabs.length > 1) {
    gBrowser.removeTab(gBrowser.selectedTab, { animate: false });
  }
});

add_task(async function autofill() {
  // Re-enable autofill and canonization.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.autoFill", true],
      ["browser.urlbar.ctrlCanonizesURLs", true],
    ],
  });

  // Quantumbar automatically disables autofill when the old search string
  // starts with the new search string, so to make sure that doesn't happen and
  // that earlier tests don't conflict with this one, start a new search for
  // some other string.
  gURLBar.select();
  EventUtils.sendString("blah");

  // Add a visit that will be autofilled.
  await PlacesUtils.history.clear();
  await PlacesTestUtils.addVisits([
    {
      uri: "http://example.com/",
      transition: PlacesUtils.history.TRANSITIONS.TYPED,
    },
  ]);

  let testcases = [
    ["ex", "http://www.ex.com/", { ctrlKey: true }],
    // Check that a direct load is not overwritten by a previous canonization.
    ["ex", "http://example.com/", {}],
    // search alias
    ["@goo", "http://www%2E@goo.com/", { ctrlKey: true }],
  ];

  function promiseAutofill() {
    return BrowserTestUtils.waitForEvent(gURLBar.inputField, "select");
  }

  for (let [inputValue, expectedURL, options] of testcases) {
    let promiseLoad = BrowserTestUtils.waitForDocLoadAndStopIt(
      expectedURL,
      gBrowser.selectedBrowser
    );
    gURLBar.select();
    let autofillPromise = promiseAutofill();
    EventUtils.sendString(inputValue);
    await autofillPromise;
    EventUtils.synthesizeKey("KEY_Enter", options);
    await promiseLoad;

    // Here again, make sure autofill isn't disabled for the next search.  See
    // the comment above.
    gURLBar.select();
    EventUtils.sendString("blah");
  }

  await PlacesUtils.history.clear();
});
