/* Any copyright is dedicated to the Public Domain.
* http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function() {
  // Test that Ctrl/Cmd + K will focus the url bar
  let focusPromise = BrowserTestUtils.waitForEvent(gURLBar, "focus");
  document.documentElement.focus();
  EventUtils.synthesizeKey("k", { accelKey: true });
  await focusPromise;
  Assert.equal(document.activeElement, gURLBar.inputField, "URL Bar should be focused");
});

