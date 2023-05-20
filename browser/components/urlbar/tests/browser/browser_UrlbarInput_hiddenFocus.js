/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function () {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);

  registerCleanupFunction(async function () {
    BrowserTestUtils.removeTab(tab);
    gURLBar.setURI();
  });

  gURLBar.blur();
  ok(!gURLBar.focused, "url bar is not focused");
  ok(!gURLBar.hasAttribute("focused"), "url bar is not visibly focused");
  gURLBar.setHiddenFocus();
  ok(gURLBar.focused, "url bar is focused");
  ok(!gURLBar.hasAttribute("focused"), "url bar is not visibly focused");
  gURLBar.removeHiddenFocus();
  ok(gURLBar.focused, "url bar is focused");
  ok(gURLBar.hasAttribute("focused"), "url bar is visibly focused");
});
