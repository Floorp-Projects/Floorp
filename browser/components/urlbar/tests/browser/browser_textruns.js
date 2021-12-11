/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * This test ensures that we limit textruns in case of very long urls or titles.
 */

add_task(async function() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.suggest.searches", true]],
  });
  await SearchTestUtils.installSearchExtension({ name: "Test" });
  let oldDefaultEngine = await Services.search.getDefault();
  await Services.search.setDefault(Services.search.getEngineByName("Test"));

  let lotsOfSpaces = "%20".repeat(300);
  await PlacesTestUtils.addVisits({
    uri: `https://textruns.mozilla.org/${lotsOfSpaces}/test/`,
    title: `A long ${lotsOfSpaces} title`,
  });
  await UrlbarTestUtils.formHistory.add([
    { value: `A long ${lotsOfSpaces} textruns suggestion` },
  ]);

  registerCleanupFunction(async function() {
    await PlacesUtils.history.clear();
    await UrlbarTestUtils.formHistory.clear();
    await Services.search.setDefault(oldDefaultEngine);
  });

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "textruns",
  });
  let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
  Assert.equal(result.searchParams.engine, "Test", "Sanity check engine");
  Assert.equal(
    result.displayed.title.length,
    UrlbarUtils.MAX_TEXT_LENGTH,
    "Result title should be limited"
  );
  result = await UrlbarTestUtils.getDetailsOfResultAt(window, 2);
  Assert.equal(
    result.displayed.title.length,
    UrlbarUtils.MAX_TEXT_LENGTH,
    "Result title should be limited"
  );
  Assert.equal(
    result.displayed.url.length,
    UrlbarUtils.MAX_TEXT_LENGTH,
    "Result url should be limited"
  );
});
