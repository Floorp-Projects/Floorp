/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the fallback paths of handleCommand (no view and no previous
 * result) work consistently against the normal case of picking the heuristic
 * result.
 */

const TEST_STRINGS = [
  "test",
  "test/",
  "test.com",
  "test.invalid",
  "moz",
  "moz test",
  "@moz test",
  "keyword",
  "keyword test",
  "test/test/",
  "test /test/",
];

add_task(async function() {
  sandbox = sinon.createSandbox();
  let engine = await Services.search.addEngineWithDetails("MozSearch", {
    alias: "moz",
    method: "GET",
    template: "http://example.com/?q={searchTerms}",
  });
  let engine2 = await Services.search.addEngineWithDetails("MozSearch2", {
    alias: "@moz",
    method: "GET",
    template: "http://example.com/?q={searchTerms}",
  });
  let bm = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: "http://example.com/?q=%s",
    title: "test",
  });
  await PlacesUtils.keywords.insert({
    keyword: "keyword",
    url: "http://example.com/?q=%s",
  });
  registerCleanupFunction(async () => {
    sandbox.restore();
    await Services.search.removeEngine(engine);
    await Services.search.removeEngine(engine2);
    await PlacesUtils.bookmarks.remove(bm);
    await UrlbarTestUtils.formHistory.clear();
  });

  async function promiseLoadURL() {
    return new Promise(resolve => {
      sandbox.stub(gURLBar, "_loadURL").callsFake(function() {
        sandbox.restore();
        // The last arguments are optional and apply only to some cases, so we
        // could not use deepEqual with them.
        resolve(Array.from(arguments).slice(0, 3));
      });
    });
  }

  // Run the string through a normal search where the user types the string
  // and confirms the heuristic result, store the arguments to _loadURL, then
  // confirm the same string without a view and without an input event, and
  // compare the arguments.
  for (let value of TEST_STRINGS) {
    let promise = promiseLoadURL();
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      waitForFocus,
      value,
    });
    EventUtils.synthesizeKey("KEY_Enter");
    let args = await promise;
    Assert.ok(args.length, "Sanity check");
    // Close the panel and confirm again.
    promise = promiseLoadURL();
    await UrlbarTestUtils.promisePopupClose(window);
    EventUtils.synthesizeKey("KEY_Enter");
    Assert.deepEqual(await promise, args, "Check arguments are coherent");
    // Set the value directly and Enter.
    promise = promiseLoadURL();
    gURLBar.value = value;
    EventUtils.synthesizeKey("KEY_Enter");
    Assert.deepEqual(await promise, args, "Check arguments are coherent");
  }
});
