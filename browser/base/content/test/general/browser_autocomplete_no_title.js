/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function* check_title(inputText, expectedTitle) {
  gURLBar.focus();
  gURLBar.value = inputText.slice(0, -1);
  EventUtils.synthesizeKey(inputText.slice(-1) , {});
  yield promiseSearchComplete();

  ok(gURLBar.popup.richlistbox.children.length > 1, "Should get at least 2 results");
  let result = gURLBar.popup.richlistbox.children[1];
  is(result._title.textContent, expectedTitle, "Result title should be as expected");
}

add_task(function*() {
  // This test is only relevant if UnifiedComplete is enabled.
  if (!Services.prefs.getBoolPref("browser.urlbar.unifiedcomplete"))
    return;

  let uri = NetUtil.newURI("http://bug1060642.example.com/beards/are/pretty/great");
  yield PlacesTestUtils.addVisits([{uri: uri, title: ""}]);

  yield check_title("bug1060642", "bug1060642.example.com");

  gURLBar.popup.hidePopup();
  yield promisePopupHidden(gURLBar.popup);
});
