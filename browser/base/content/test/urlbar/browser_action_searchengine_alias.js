/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function() {
  let iconURI = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAIAAACQkWg2AAABGklEQVQoz2NgGB6AnZ1dUlJSXl4eSDIyMhLW4Ovr%2B%2Fr168uXL69Zs4YoG%2BLi4i5dusTExMTGxsbNzd3f37937976%2BnpmZmagbHR09J49e5YvX66kpATVEBYW9ubNm2nTphkbG7e2tp44cQLIuHfvXm5urpaWFlDKysqqu7v73LlzECMYIiIiHj58mJCQoKKicvXq1bS0NKBgW1vbjh074uPjgeqAXE1NzSdPnvDz84M0AEUvXLgAsW379u1z5swBen3jxo2zZ892cHB4%2BvQp0KlAfwI1cHJyghQFBwfv2rULokFXV%2FfixYu7d%2B8GGqGgoMDKyrpu3br9%2B%2FcDuXl5eVA%2FAEWBfoWHAdAYoNuAYQ0XAeoUERFhGDYAAPoUaT2dfWJuAAAAAElFTkSuQmCC";
  Services.search.addEngineWithDetails("MozSearch", iconURI, "moz", "", "GET",
                                       "http://example.com/?q={searchTerms}");
  let engine = Services.search.getEngineByName("MozSearch");
  let originalEngine = Services.search.currentEngine;
  Services.search.currentEngine = engine;

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:mozilla");

  registerCleanupFunction(async function() {
    Services.search.currentEngine = originalEngine;
    Services.search.removeEngine(engine);
    try {
      await BrowserTestUtils.removeTab(tab);
    } catch (ex) { /* tab may have already been closed in case of failure */ }
    await PlacesUtils.history.clear();
  });

  await promiseAutocompleteResultPopup("moz open a search");
  let result = await waitForAutocompleteResultAt(0);
  ok(result.hasAttribute("image"), "Result should have an image attribute");
  ok(result.getAttribute("image") === engine.iconURI.spec,
     "Image attribute should have the search engine's icon");

  let tabPromise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  EventUtils.synthesizeKey("VK_RETURN", { });
  await tabPromise;

  is(gBrowser.selectedBrowser.currentURI.spec, "http://example.com/?q=open+a+search");

  gURLBar.popup.hidePopup();
  await promisePopupHidden(gURLBar.popup);
});
