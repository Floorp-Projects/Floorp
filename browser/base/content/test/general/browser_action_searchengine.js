/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

let gOriginalEngine;

function* promise_first_result(inputText) {
  gURLBar.focus();
  gURLBar.value = inputText.slice(0, -1);
  EventUtils.synthesizeKey(inputText.slice(-1) , {});
  yield promiseSearchComplete();
  // On Linux, the popup may or may not be open at this stage. So we need
  // additional checks to ensure we wait long enough.
  yield promisePopupShown(gURLBar.popup);

  let firstResult = gURLBar.popup.richlistbox.firstChild;
  return firstResult;
}

add_task(function* () {
  // This test is only relevant if UnifiedComplete is enabled.
  if (!Services.prefs.getBoolPref("browser.urlbar.unifiedcomplete"))
    return;

  Services.search.addEngineWithDetails("MozSearch", "", "", "", "GET",
                                       "http://example.com/?q={searchTerms}");
  let engine = Services.search.getEngineByName("MozSearch");
  gOriginalEngine = Services.search.currentEngine;
  Services.search.currentEngine = engine;

  let tab = gBrowser.selectedTab = gBrowser.addTab();

  registerCleanupFunction(() => {
    Services.search.currentEngine = gOriginalEngine;
    let engine = Services.search.getEngineByName("MozSearch");
    Services.search.removeEngine(engine);

    try {
      gBrowser.removeTab(tab);
    } catch(ex) { /* tab may have already been closed in case of failure */ }

    return promiseClearHistory();
  });

  let result = yield promise_first_result("open a search");
  isnot(result, null, "Should have a result");
  is(result.hasAttribute("image"), false, "Result shouldn't have an image attribute");

  let tabPromise = promiseTabLoaded(gBrowser.selectedTab);
  EventUtils.synthesizeMouseAtCenter(result, {});
  yield tabPromise;

  is(gBrowser.selectedBrowser.currentURI.spec, "http://example.com/?q=open+a+search");
});
