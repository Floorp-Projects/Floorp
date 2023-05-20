/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var gTests = [
  {
    name: "single word search (search service)",
    text: "pizza",
    expectText: "pizza",
  },
  {
    name: "multi word search (search service)",
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
  await SearchTestUtils.promiseNewSearchEngine({
    url: getRootDirectory(gTestPath) + "POSTSearchEngine.xml",
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

        await BrowserTestUtils.browserLoaded(
          browser,
          false,
          "http://mochi.test:8888/browser/browser/components/urlbar/tests/browser/print_postdata.sjs"
        );

        let textContent = await SpecialPowers.spawn(browser, [], async () => {
          return content.document.body.textContent;
        });

        Assert.ok(textContent, "search page loaded");
        let needle = "searchterms=" + test.expectText;
        Assert.equal(
          textContent,
          needle,
          "The query POST data should be returned in the response"
        );
      }
    });
  }
});
