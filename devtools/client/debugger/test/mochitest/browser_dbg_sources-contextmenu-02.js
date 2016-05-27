/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests the "Open in New Tab" functionality of the sources panel context menu
 */

const TAB_URL = EXAMPLE_URL + "doc_function-search.html";
const SCRIPT_URI = EXAMPLE_URL + "code_function-search-01.js";

function test() {
  let gTab, gPanel, gDebugger;
  let gSources;

  let options = {
    source: SCRIPT_URI,
    line: 1
  };
  initDebugger(TAB_URL, options).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gSources = gDebugger.DebuggerView.Sources;

    openContextMenu()
      .then(testNewTabMenuItem)
      .then(testNewTabURI)
      .then(() => closeDebuggerAndFinish(gPanel))
      .then(null, aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });
  });

  function testNewTabURI(tabUri) {
    is(tabUri, SCRIPT_URI, "The tab contains the right script.");
    gBrowser.removeCurrentTab();
  }

  function waitForTabOpen() {
    return new Promise(resolve => {
      gBrowser.tabContainer.addEventListener("TabOpen", function onOpen(e) {
        gBrowser.tabContainer.removeEventListener("TabOpen", onOpen, false);
        ok(true, "A new tab loaded");

        gBrowser.addEventListener("DOMContentLoaded", function onTabLoad(e) {
          gBrowser.removeEventListener("DOMContentLoaded", onTabLoad, false);
          // Pass along the new tab's URI.
          resolve(gBrowser.currentURI.spec);
        }, false);
      }, false);
    });
  }

  function testNewTabMenuItem() {
    return new Promise((resolve, reject) => {
      let newTabMenuItem = gDebugger.document.getElementById("debugger-sources-context-newtab");
      if (!newTabMenuItem) {
        reject(new Error("The Open in New Tab context menu item is not available."));
      }

      ok(newTabMenuItem, "The Open in New Tab context menu item is available.");
      waitForTabOpen().then(resolve);
      EventUtils.synthesizeMouseAtCenter(newTabMenuItem, {}, gDebugger);
    });
  }

  function openContextMenu() {
    let contextMenu = gDebugger.document.getElementById("debuggerSourcesContextMenu");
    let contextMenuShown = once(contextMenu, "popupshown");
    EventUtils.synthesizeMouseAtCenter(gSources.selectedItem.prebuiltNode, {type: "contextmenu"}, gDebugger);
    return contextMenuShown;
  }
}
