/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_ENGINE_BASENAME = "searchSuggestionEngine.xml";

add_task(async function checkCtrlWorks() {
  let testcases = [
    ["example", "http://www.example.com/", { ctrlKey: true }],
    // Check that a direct load is not overwritten by a previous canonization.
    ["http://example.com/test/", "http://example.com/test/", {}],

    ["ex-ample", "http://www.ex-ample.com/", { ctrlKey: true }],
    ["  example ", "http://www.example.com/", { ctrlKey: true }],
    [" example/foo ", "http://www.example.com/foo", { ctrlKey: true }],
    [" example/foo bar ", "http://www.example.com/foo%20bar", { ctrlKey: true }],
    ["example.net", "http://example.net/", { ctrlKey: true }],
    ["http://example", "http://example/", { ctrlKey: true }],
    ["example:8080", "http://example:8080/", { ctrlKey: true }],
    ["ex-ample.foo", "http://ex-ample.foo/", { ctrlKey: true }],
    ["example.foo/bar ", "http://example.foo/bar", { ctrlKey: true }],
    ["1.1.1.1", "http://1.1.1.1/", { ctrlKey: true }],
    ["ftp://example", "ftp://example/", { ctrlKey: true }],
    ["ftp.example.bar", "http://ftp.example.bar/", { ctrlKey: true }],
    ["ex ample", Services.search.defaultEngine.getSubmission("ex ample", null, "keyword").uri.spec, { ctrlKey: true }],
  ];

  // Disable autoFill for this test, since it could mess up the results.
  await SpecialPowers.pushPrefEnv({set: [
    ["browser.urlbar.autoFill", false],
    ["browser.urlbar.ctrlCanonizesURLs", true],
  ]});

  for (let [inputValue, expectedURL, options] of testcases) {
    let promiseLoad =
      BrowserTestUtils.waitForDocLoadAndStopIt(expectedURL, gBrowser.selectedBrowser);
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
    getRootDirectory(gTestPath) + TEST_ENGINE_BASENAME);
  let oldCurrentEngine = Services.search.defaultEngine;
  Services.search.defaultEngine = engine;
  registerCleanupFunction(() => { Services.search.defaultEngine = oldCurrentEngine; });

  let tabsToClose = [];
  // Ensure we don't end up loading something in the current tab becuase it's empty:
  if (gBrowser.selectedTab.isEmpty) {
    tabsToClose.push(await BrowserTestUtils.openNewForegroundTab({gBrowser, opening: "about:mozilla"}));
  }
  let initialTabURL = gBrowser.selectedBrowser.currentURI.spec;
  let initialTab = gBrowser.selectedTab;
  await SpecialPowers.pushPrefEnv({set: [["browser.urlbar.ctrlCanonizesURLs", false]]});

  let promiseTabOpened = BrowserTestUtils.waitForNewTab(gBrowser);

  gURLBar.focus();
  gURLBar.selectionStart = gURLBar.selectionEnd =
    gURLBar.inputField.value.length;
  gURLBar.inputField.value = "exampl";
  EventUtils.sendString("e");
  EventUtils.synthesizeKey("KEY_Enter", AppConstants.platform == "macosx" ?
                           {metaKey: true} : {ctrlKey: true});

  tabsToClose.push(await promiseTabOpened);
  is(initialTab.linkedBrowser.currentURI.spec, initialTabURL,
     "Original tab shouldn't have navigated");
  for (let t of tabsToClose) {
    gBrowser.removeTab(t);
  }
});
