/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Ensure that we can restore old style favicon and principals.
 */
add_task(async function test_label_and_icon() {
  // Make sure that tabs are restored on demand as otherwise the tab will start
  // loading immediately and override the icon.
  await SpecialPowers.pushPrefEnv({
    set: [["browser.sessionstore.restore_on_demand", true]],
  });

  // Create a new tab.
  let tab = BrowserTestUtils.addTab(
    gBrowser,
    "http://www.example.com/browser/browser/components/sessionstore/test/empty.html"
  );
  let browser = tab.linkedBrowser;
  await promiseBrowserLoaded(browser);

  let contentPrincipal = browser.contentPrincipal;
  let serializedPrincipal = E10SUtils.serializePrincipal(contentPrincipal);

  // Retrieve the tab state.
  await TabStateFlusher.flush(browser);
  let state = JSON.parse(ss.getTabState(tab));
  state.image = "http://www.example.com/favicon.ico";
  state.iconLoadingPrincipal = serializedPrincipal;

  BrowserTestUtils.removeTab(tab);

  // Open a new tab to restore into.
  tab = BrowserTestUtils.addTab(gBrowser, "about:blank");
  ss.setTabState(tab, state);
  await promiseTabRestoring(tab);

  // Check that label and icon are set for the restoring tab.
  is(
    gBrowser.getIcon(tab),
    "http://www.example.com/favicon.ico",
    "icon is set"
  );
  is(
    tab.getAttribute("image"),
    "http://www.example.com/favicon.ico",
    "tab image is set"
  );
  is(
    tab.getAttribute("iconloadingprincipal"),
    serializedPrincipal,
    "tab image loading principal is set"
  );

  // Cleanup.
  BrowserTestUtils.removeTab(tab);
});
