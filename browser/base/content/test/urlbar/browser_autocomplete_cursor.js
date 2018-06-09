/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:mozilla");
  await promiseAutocompleteResultPopup("www.mozilla.org");
  await waitForAutocompleteResultAt(0);

  gURLBar.selectTextRange(4, 4);

  is(gURLBar.popup.state, "open", "Popup should be open");
  is(gURLBar.popup.richlistbox.selectedIndex, 0, "Should have selected something");

  EventUtils.synthesizeKey("KEY_ArrowRight");
  await promisePopupHidden(gURLBar.popup);

  is(gURLBar.selectionStart, 5, "Should have moved the cursor");
  is(gURLBar.selectionEnd, 5, "And not selected anything");

  BrowserTestUtils.removeTab(tab);
});
