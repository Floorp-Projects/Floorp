/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);

  registerCleanupFunction(async function() {
    BrowserTestUtils.removeTab(tab);
    URLBarSetURI();
  });

  gURLBar.blur();
  ok(
    !gURLBar.hasAttribute("focused") &&
      !gURLBar.textbox.classList.contains("hidden-focus"),
    "url bar is not focused or hidden"
  );
  gURLBar.setHiddenFocus();
  ok(
    gURLBar.hasAttribute("focused") &&
      gURLBar.textbox.classList.contains("hidden-focus"),
    "url bar is focused and hidden"
  );
  gURLBar.removeHiddenFocus();
  ok(
    gURLBar.hasAttribute("focused") &&
      !gURLBar.textbox.classList.contains("hidden-focus"),
    "url bar is focused and not hidden"
  );
});
