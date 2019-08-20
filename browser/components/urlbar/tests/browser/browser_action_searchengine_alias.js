/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that search result obtained using a search keyword gives an entry with
 * the correct attributes and visits the expected URL for the engine.
 */

add_task(async function() {
  const ICON_URI =
    "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAIAAA" +
    "CQkWg2AAABGklEQVQoz2NgGB6AnZ1dUlJSXl4eSDIyMhLW4Ovr%2B%2Fr168uXL69Zs4YoG%2BL" +
    "i4i5dusTExMTGxsbNzd3f37937976%2BnpmZmagbHR09J49e5YvX66kpATVEBYW9ubNm2nTphkb" +
    "G7e2tp44cQLIuHfvXm5urpaWFlDKysqqu7v73LlzECMYIiIiHj58mJCQoKKicvXq1bS0NKBgW1v" +
    "bjh074uPjgeqAXE1NzSdPnvDz84M0AEUvXLgAsW379u1z5swBen3jxo2zZ892cHB4%2BvQp0KlA" +
    "fwI1cHJyghQFBwfv2rULokFXV%2FfixYu7d%2B8GGqGgoMDKyrpu3br9%2B%2FcDuXl5eVA%2FA" +
    "EWBfoWHAdAYoNuAYQ0XAeoUERFhGDYAAPoUaT2dfWJuAAAAAElFTkSuQmCC";
  await Services.search.addEngineWithDetails("MozSearch", {
    iconURL: ICON_URI,
    alias: "moz",
    method: "GET",
    template: "http://example.com/?q={searchTerms}",
  });
  let engine = Services.search.getEngineByName("MozSearch");
  let originalEngine = await Services.search.getDefault();
  await Services.search.setDefault(engine);

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:mozilla"
  );

  registerCleanupFunction(async function() {
    await Services.search.setDefault(originalEngine);
    await Services.search.removeEngine(engine);
    try {
      BrowserTestUtils.removeTab(tab);
    } catch (ex) {
      /* tab may have already been closed in case of failure */
    }
    await PlacesUtils.history.clear();
  });

  await promiseAutocompleteResultPopup("moz");
  Assert.equal(
    gURLBar.value,
    "moz",
    "Preselected search keyword result shouldn't automatically add a space"
  );

  await promiseAutocompleteResultPopup("moz open a search");
  let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.equal(result.image, ICON_URI, "Should have the correct image");

  let tabPromise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  EventUtils.synthesizeKey("KEY_Enter");
  await tabPromise;

  Assert.equal(
    gBrowser.selectedBrowser.currentURI.spec,
    "http://example.com/?q=open+a+search",
    "Should have loaded the correct page"
  );
});
