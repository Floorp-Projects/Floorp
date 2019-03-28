/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

add_task(async function() {
  await BrowserTestUtils.withNewTab({ gBrowser, url: "http://example.com/" }, async () => {
    await UrlbarTestUtils.promisePopupOpen(window, () => {
      let historyDropMarker =
        window.document.getAnonymousElementByAttribute(gURLBar.textbox, "anonid", "historydropmarker");
      EventUtils.synthesizeMouseAtCenter(historyDropMarker, {}, window);
    });
    let queryContext = await gURLBar.lastQueryContextPromise;
    is(queryContext.searchString, "",
       "Clicking the history dropmarker should initiate an empty search instead of searching for the loaded URL");
    await UrlbarTestUtils.promisePopupClose(window);
  });
});
