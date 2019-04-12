/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * This test ensures that we limit textruns in case of very long urls or titles.
 */

 add_task(async function() {
  let lotsOfSpaces = "%20".repeat(300);
  await PlacesTestUtils.addVisits({
    uri: `https://textruns.mozilla.org/${lotsOfSpaces}/test/`,
    title: `A long ${lotsOfSpaces} title`});
  registerCleanupFunction(async function() {
    await PlacesUtils.history.clear();
  });

  await promiseAutocompleteResultPopup("textruns");
  let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
  Assert.equal(result.displayed.title.length, UrlbarUtils.MAX_TEXT_LENGTH,
               "Result title should be limited");
  Assert.equal(result.displayed.url.length, UrlbarUtils.MAX_TEXT_LENGTH,
               "Result url should be limited");
});
