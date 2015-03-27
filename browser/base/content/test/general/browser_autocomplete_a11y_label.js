/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(function*() {
  // This test is only relevant if UnifiedComplete is enabled.
  let ucpref = Services.prefs.getBoolPref("browser.urlbar.unifiedcomplete");
  Services.prefs.setBoolPref("browser.urlbar.unifiedcomplete", true);
  registerCleanupFunction(() => {
    Services.prefs.setBoolPref("browser.urlbar.unifiedcomplete", ucpref);
  });

  let tab = gBrowser.addTab("about:about");
  yield promiseTabLoaded(tab);

  let actionURL = makeActionURI("switchtab", {url: "about:about"}).spec;
  yield promiseAutocompleteResultPopup("% about");

  ok(gURLBar.popup.richlistbox.children.length > 1, "Should get at least 2 results");
  let result = gURLBar.popup.richlistbox.children[1];
  is(result.getAttribute("type"), "action switchtab", "Expect right type attribute");
  is(result.label, "about:about " + actionURL + " Tab", "Result a11y label should be as expected");

  gURLBar.popup.hidePopup();
  yield promisePopupHidden(gURLBar.popup);
  gBrowser.removeTab(tab);
});
