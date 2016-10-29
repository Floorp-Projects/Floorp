/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(function*() {

  // Remove the search bar from toolbar
  CustomizableUI.removeWidgetFromArea("search-container");

  // Test that Ctrl/Cmd + K will focus the url bar
  yield EventUtils.synthesizeKey("k", { accelKey: true });
  yield BrowserTestUtils.waitForEvent(gURLBar, "focus");
  Assert.equal(document.activeElement, gURLBar.inputField, "URL Bar should be focused");

  // Reset changes made to toolbar
  CustomizableUI.reset();
});

