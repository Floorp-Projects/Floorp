/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var gTests = [
  {
    name: "normal search (search service)",
    text: "test search",
    expectText: "test+search",
  },
  {
    name: "?-prefixed search (search service)",
    text: "?   foo  ",
    expectText: "foo",
  },
];

add_setup(async function () {
  await SearchTestUtils.installOpenSearchEngine({
    url: getRootDirectory(gTestPath) + "searchSuggestionEngine.xml",
    setAsDefault: true,
  });
});

add_task(async function () {
  // Test both directly setting a value and pressing enter, or setting the
  // value through input events, like the user would do.
  const setValueFns = [
    value => {
      gURLBar.value = value;
    },
    value => {
      return UrlbarTestUtils.promiseAutocompleteResultPopup({
        window,
        value,
      });
    },
  ];

  for (let test of gTests) {
    info("Testing: " + test.name);
    await BrowserTestUtils.withNewTab({ gBrowser }, async browser => {
      for (let setValueFn of setValueFns) {
        gURLBar.select();
        await setValueFn(test.text);
        EventUtils.synthesizeKey("KEY_Enter");

        let expectedUrl = "http://mochi.test:8888/?terms=" + test.expectText;
        info("Waiting for load: " + expectedUrl);
        await BrowserTestUtils.browserLoaded(browser, false, expectedUrl);
        // At least one test.
        Assert.equal(browser.currentURI.spec, expectedUrl);
      }
    });
  }
});
