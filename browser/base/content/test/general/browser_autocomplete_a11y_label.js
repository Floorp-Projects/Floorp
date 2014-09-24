/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function* check_a11y_label(inputText, expectedLabel) {
  gURLBar.focus();
  gURLBar.value = inputText.slice(0, -1);
  EventUtils.synthesizeKey(inputText.slice(-1) , {});
  yield promiseSearchComplete();
  // On Linux, the popup may or may not be open at this stage. So we need
  // additional checks to ensure we wait long enough.
  yield promisePopupShown(gURLBar.popup);

  let firstResult = gURLBar.popup.richlistbox.firstChild;
  is(firstResult.getAttribute("type"), "action switchtab", "Expect right type attribute");
  is(firstResult.label, expectedLabel, "Result a11y label should be as expected");
}

add_task(function*() {
  // This test is only relevant if UnifiedComplete is enabled.
  if (!Services.prefs.getBoolPref("browser.urlbar.unifiedcomplete"))
    return;

  let tab = gBrowser.addTab("about:about");
  yield promiseTabLoaded(tab);

  let actionURL = makeActionURI("switchtab", {url: "about:about"}).spec;
  yield check_a11y_label("% about", "about:about " + actionURL + " Tab");

  yield promisePopupHidden(gURLBar.popup);
  gBrowser.removeTab(tab);
});
