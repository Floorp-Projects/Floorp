/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  TestRunner.run();
}

/**
 * This test ensures that disabling features by flipping nsIDocShell.allow*
 * properties are (re)stored as disabled. Disallowed features must be
 * re-enabled when the tab is re-used by another tab restoration.
 */

function runTests() {
  // Create a tab that we're going to use for our tests.
  let tab = gBrowser.selectedTab = gBrowser.addTab("about:mozilla");
  let browser = tab.linkedBrowser;
  let docShell = browser.docShell;
  yield waitForLoad(browser);

  // Get the list of capabilities for docShells.
  let flags = Object.keys(docShell).filter(k => k.startsWith("allow"));

  // Check that everything is allowed by default for new tabs.
  let state = JSON.parse(ss.getTabState(tab));
  ok(!("disallow" in state), "everything allowed by default");
  ok(flags.every(f => docShell[f]), "all flags set to true");

  // Flip a couple of allow* flags.
  docShell.allowImages = false;
  docShell.allowMetaRedirects = false;

  // Check that we correctly save disallowed features.
  let disallowedState = JSON.parse(ss.getTabState(tab));
  let disallow = new Set(disallowedState.disallow.split(","));
  ok(disallow.has("Images"), "images not allowed");
  ok(disallow.has("MetaRedirects"), "meta redirects not allowed");
  is(disallow.size, 2, "two capabilities disallowed");

  // Reuse the tab to restore a new, clean state into it.
  ss.setTabState(tab, JSON.stringify({ entries: [{url: "about:robots"}] }));
  yield waitForLoad(browser);

  // After restoring disallowed features must be available again.
  state = JSON.parse(ss.getTabState(tab));
  ok(!("disallow" in state), "everything allowed again");
  ok(flags.every(f => docShell[f]), "all flags set to true");

  // Restore the state with disallowed features.
  ss.setTabState(tab, JSON.stringify(disallowedState));
  yield waitForLoad(browser);

  // Check that docShell flags are set.
  ok(!docShell.allowImages, "images not allowed");
  ok(!docShell.allowMetaRedirects, "meta redirects not allowed")

  // Check that we correctly restored features as disabled.
  state = JSON.parse(ss.getTabState(tab));
  disallow = new Set(state.disallow.split(","));
  ok(disallow.has("Images"), "images not allowed anymore");
  ok(disallow.has("MetaRedirects"), "meta redirects not allowed anymore");
  is(disallow.size, 2, "two capabilities disallowed");

  // Clean up after ourselves.
  gBrowser.removeTab(tab);
}

function waitForLoad(aElement) {
  aElement.addEventListener("load", function onLoad() {
    aElement.removeEventListener("load", onLoad, true);
    executeSoon(next);
  }, true);
}
