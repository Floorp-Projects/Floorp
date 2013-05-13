/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  TestRunner.run();
}

/**
 * This test makes sure that we correctly preserve tab attributes when storing
 * and restoring tabs. It also ensures that we skip special attributes like
 * 'image' and 'pending' that need to be handled differently or internally.
 */

const PREF = "browser.sessionstore.restore_on_demand";

function runTests() {
  Services.prefs.setBoolPref(PREF, true)
  registerCleanupFunction(() => Services.prefs.clearUserPref(PREF));

  // Add a new tab with a nice icon.
  let tab = gBrowser.addTab("about:robots");
  yield whenBrowserLoaded(tab.linkedBrowser);

  // Check that the tab has an 'image' attribute.
  ok(tab.hasAttribute("image"), "tab.image exists");

  // Make sure we do not persist 'image' attributes.
  ss.persistTabAttribute("image");
  let {attributes} = JSON.parse(ss.getTabState(tab));
  ok(!("image" in attributes), "'image' attribute not saved");
  ok(!("custom" in attributes), "'custom' attribute not saved");

  // Test persisting a custom attribute.
  tab.setAttribute("custom", "foobar");
  ss.persistTabAttribute("custom");

  let {attributes} = JSON.parse(ss.getTabState(tab));
  is(attributes.custom, "foobar", "'custom' attribute is correct");

  // Make sure we're backwards compatible and restore old 'image' attributes.
  let state = {
    entries: [{url: "about:mozilla"}],
    attributes: {custom: "foobaz", image: gBrowser.getIcon(tab)}
  };

  // Prepare a pending tab waiting to be restored.
  whenTabRestoring(tab);
  yield ss.setTabState(tab, JSON.stringify(state));

  ok(tab.hasAttribute("pending"), "tab is pending");
  is(gBrowser.getIcon(tab), state.attributes.image, "tab has correct icon");

  // Let the pending tab load.
  gBrowser.selectedTab = tab;
  yield whenBrowserLoaded(tab.linkedBrowser);

  // Ensure no 'image' or 'pending' attributes are stored.
  let {attributes} = JSON.parse(ss.getTabState(tab));
  ok(!("image" in attributes), "'image' attribute not saved");
  ok(!("pending" in attributes), "'pending' attribute not saved");
  is(attributes.custom, "foobaz", "'custom' attribute is correct");

  // Clean up.
  gBrowser.removeTab(tab);
}

function whenTabRestoring(tab) {
  tab.addEventListener("SSTabRestoring", function onRestoring() {
    tab.removeEventListener("SSTabRestoring", onRestoring);
    executeSoon(next);
  });
}
