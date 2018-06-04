/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This test makes sure that we correctly preserve tab attributes when storing
 * and restoring tabs. It also ensures that we skip special attributes like
 * 'image', 'muted', and 'pending' that need to be
 * handled differently or internally.
 */

const PREF = "browser.sessionstore.restore_on_demand";

add_task(async function test() {
  Services.prefs.setBoolPref(PREF, true);
  registerCleanupFunction(() => Services.prefs.clearUserPref(PREF));

  // Add a new tab with a nice icon.
  let tab = BrowserTestUtils.addTab(gBrowser, "about:robots");
  await promiseBrowserLoaded(tab.linkedBrowser);

  // Because there is debounce logic in ContentLinkHandler.jsm to reduce the
  // favicon loads, we have to wait some time before checking that icon was
  // stored properly.
  await BrowserTestUtils.waitForCondition(() => {
    return gBrowser.getIcon(tab) != null;
  }, "wait for favicon load to finish", 100, 5);

  // Check that the tab has an 'image' attribute.
  ok(tab.hasAttribute("image"), "tab.image exists");

  tab.toggleMuteAudio();
  // Check that the tab has a 'muted' attribute.
  ok(tab.hasAttribute("muted"), "tab.muted exists");

  // Make sure we do not persist 'image' and 'muted' attributes.
  ss.persistTabAttribute("image");
  ss.persistTabAttribute("muted");
  let {attributes} = JSON.parse(ss.getTabState(tab));
  ok(!("image" in attributes), "'image' attribute not saved");
  ok(!("muted" in attributes), "'muted' attribute not saved");
  ok(!("custom" in attributes), "'custom' attribute not saved");

  // Test persisting a custom attribute.
  tab.setAttribute("custom", "foobar");
  ss.persistTabAttribute("custom");

  ({attributes} = JSON.parse(ss.getTabState(tab)));
  is(attributes.custom, "foobar", "'custom' attribute is correct");

  // Make sure we're backwards compatible and restore old 'image' attributes.
  let state = {
    entries: [{url: "about:mozilla", triggeringPrincipal_base64 }],
    attributes: {custom: "foobaz"},
    image: gBrowser.getIcon(tab)
  };

  // Prepare a pending tab waiting to be restored.
  let promise = promiseTabRestoring(tab);
  ss.setTabState(tab, JSON.stringify(state));
  await promise;

  ok(tab.hasAttribute("pending"), "tab is pending");
  is(gBrowser.getIcon(tab), state.image, "tab has correct icon");
  ok(!state.attributes.image, "'image' attribute not saved");

  // Let the pending tab load.
  gBrowser.selectedTab = tab;
  await promiseTabRestored(tab);

  // Ensure no 'image' or 'pending' attributes are stored.
  ({attributes} = JSON.parse(ss.getTabState(tab)));
  ok(!("image" in attributes), "'image' attribute not saved");
  ok(!("pending" in attributes), "'pending' attribute not saved");
  is(attributes.custom, "foobaz", "'custom' attribute is correct");

  // Clean up.
  gBrowser.removeTab(tab);
});
