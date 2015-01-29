/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * These tests ensures that disabling features by flipping nsIDocShell.allow*
 * properties are (re)stored as disabled. Disallowed features must be
 * re-enabled when the tab is re-used by another tab restoration.
 */
add_task(function docshell_capabilities() {
  let tab = yield createTab();
  let browser = tab.linkedBrowser;
  let docShell = browser.docShell;

  // Get the list of capabilities for docShells.
  let flags = Object.keys(docShell).filter(k => k.startsWith("allow"));

  // Check that everything is allowed by default for new tabs.
  let state = JSON.parse(ss.getTabState(tab));
  ok(!("disallow" in state), "everything allowed by default");
  ok(flags.every(f => docShell[f]), "all flags set to true");

  // Flip a couple of allow* flags.
  docShell.allowImages = false;
  docShell.allowMetaRedirects = false;

  // Now reload the document to ensure that these capabilities
  // are taken into account.
  browser.reload();
  yield promiseBrowserLoaded(browser);

  // Flush to make sure chrome received all data.
  TabState.flush(browser);

  // Check that we correctly save disallowed features.
  let disallowedState = JSON.parse(ss.getTabState(tab));
  let disallow = new Set(disallowedState.disallow.split(","));
  ok(disallow.has("Images"), "images not allowed");
  ok(disallow.has("MetaRedirects"), "meta redirects not allowed");
  is(disallow.size, 2, "two capabilities disallowed");

  // Reuse the tab to restore a new, clean state into it.
  yield promiseTabState(tab, {entries: [{url: "about:robots"}]});

  // Flush to make sure chrome received all data.
  TabState.flush(browser);

  // After restoring disallowed features must be available again.
  state = JSON.parse(ss.getTabState(tab));
  ok(!("disallow" in state), "everything allowed again");
  ok(flags.every(f => docShell[f]), "all flags set to true");

  // Restore the state with disallowed features.
  yield promiseTabState(tab, disallowedState);

  // Check that docShell flags are set.
  ok(!docShell.allowImages, "images not allowed");
  ok(!docShell.allowMetaRedirects, "meta redirects not allowed");

  // Check that we correctly restored features as disabled.
  state = JSON.parse(ss.getTabState(tab));
  disallow = new Set(state.disallow.split(","));
  ok(disallow.has("Images"), "images not allowed anymore");
  ok(disallow.has("MetaRedirects"), "meta redirects not allowed anymore");
  is(disallow.size, 2, "two capabilities disallowed");

  // Clean up after ourselves.
  gBrowser.removeTab(tab);
});

function createTab() {
  let tab = gBrowser.addTab("about:mozilla");
  let browser = tab.linkedBrowser;
  return promiseBrowserLoaded(browser).then(() => tab);
}
