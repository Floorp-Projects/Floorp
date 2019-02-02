/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);

  registerCleanupFunction(async function() {
    BrowserTestUtils.removeTab(tab);
  });

  // Test with a search shortcut.
  gURLBar.blur();
  gURLBar.search("@google ");
  ok(gURLBar.hasAttribute("focused"), "url bar is focused");
  is(gURLBar.value, "@google ", "url bar has correct value");

  // Test with a restricted token.
  gURLBar.blur();
  gURLBar.search("?");
  ok(gURLBar.hasAttribute("focused"), "url bar is focused");
  is(gURLBar.value, "? ", "url bar has correct value");
});
