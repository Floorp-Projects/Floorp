/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let contextMenu;

const example_base =
  "http://example.com/browser/browser/base/content/test/contextMenu/";
const MAIN_URL = example_base + "subtst_contextmenu_input.html";

add_task(async function test_setup() {
  await BrowserTestUtils.openNewForegroundTab(gBrowser, MAIN_URL);

  const chrome_base =
    "chrome://mochitests/content/browser/browser/base/content/test/contextMenu/";
  const contextmenu_common = chrome_base + "contextmenu_common.js";
  /* import-globals-from contextmenu_common.js */
  Services.scriptloader.loadSubScript(contextmenu_common, this);
});

add_task(async function test_text_input_spellcheck() {
  await test_contextmenu(
    "#input_spellcheck_no_value",
    [
      "context-undo",
      false,
      "context-redo",
      false,
      "---",
      null,
      "context-cut",
      null, // ignore the enabled/disabled states; there are race conditions
      // in the edit commands but they're not relevant for what we're testing.
      "context-copy",
      null,
      "context-paste",
      null, // ignore clipboard state
      "context-delete",
      null,
      "context-selectall",
      null,
      "---",
      null,
      "spell-check-enabled",
      true,
      "spell-dictionaries",
      true,
      [
        "spell-check-dictionary-en-US",
        true,
        "---",
        null,
        "spell-add-dictionaries",
        true,
      ],
      null,
    ],
    {
      waitForSpellCheck: true,
      async preCheckContextMenuFn() {
        await SpecialPowers.spawn(
          gBrowser.selectedBrowser,
          [],
          async function() {
            let doc = content.document;
            let input = doc.getElementById("input_spellcheck_no_value");
            input.setAttribute("spellcheck", "true");
            input.clientTop; // force layout flush
          }
        );
      },
    }
  );
});

add_task(async function test_text_input_spellcheckwrong() {
  await test_contextmenu(
    "#input_spellcheck_incorrect",
    [
      "*prodigality",
      true, // spelling suggestion
      "spell-add-to-dictionary",
      true,
      "---",
      null,
      "context-undo",
      null,
      "context-redo",
      null,
      "---",
      null,
      "context-cut",
      null,
      "context-copy",
      null,
      "context-paste",
      null, // ignore clipboard state
      "context-delete",
      null,
      "context-selectall",
      null,
      "---",
      null,
      "spell-check-enabled",
      true,
      "spell-dictionaries",
      true,
      [
        "spell-check-dictionary-en-US",
        true,
        "---",
        null,
        "spell-add-dictionaries",
        true,
      ],
      null,
    ],
    { waitForSpellCheck: true }
  );
});

const kCorrectItems = [
  "context-undo",
  false,
  "context-redo",
  false,
  "---",
  null,
  "context-cut",
  null,
  "context-copy",
  null,
  "context-paste",
  null, // ignore clipboard state
  "context-delete",
  null,
  "context-selectall",
  null,
  "---",
  null,
  "spell-check-enabled",
  true,
  "spell-dictionaries",
  true,
  [
    "spell-check-dictionary-en-US",
    true,
    "---",
    null,
    "spell-add-dictionaries",
    true,
  ],
  null,
];

add_task(async function test_text_input_spellcheckcorrect() {
  await test_contextmenu("#input_spellcheck_correct", kCorrectItems, {
    waitForSpellCheck: true,
  });
});

add_task(async function test_text_input_spellcheck_deadactor() {
  await test_contextmenu("#input_spellcheck_correct", kCorrectItems, {
    waitForSpellCheck: true,
    keepMenuOpen: true,
  });
  let wgp = gBrowser.selectedBrowser.browsingContext.currentWindowGlobal;

  // Now the menu is open, and spellcheck is running, switch to another tab and
  // close the original:
  let tab = gBrowser.selectedTab;
  await BrowserTestUtils.openNewForegroundTab(gBrowser, "https://example.org");
  BrowserTestUtils.removeTab(tab);
  // Ensure we've invalidated the actor
  await TestUtils.waitForCondition(
    () => wgp.isClosed,
    "Waiting for actor to be dead after tab closes"
  );
  contextMenu.hidePopup();

  // Now go back to the input testcase:
  BrowserTestUtils.loadURI(gBrowser.selectedBrowser, MAIN_URL);
  await BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    MAIN_URL
  );

  // Check the menu still looks the same, keep it open again:
  await test_contextmenu("#input_spellcheck_correct", kCorrectItems, {
    waitForSpellCheck: true,
    keepMenuOpen: true,
  });

  // Now navigate the tab, after ensuring there's an unload listener, so
  // we don't end up in bfcache:
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function() {
    content.document.body.setAttribute("onunload", "");
  });
  wgp = gBrowser.selectedBrowser.browsingContext.currentWindowGlobal;

  const NEW_URL = MAIN_URL.replace(".com", ".org");
  BrowserTestUtils.loadURI(gBrowser.selectedBrowser, NEW_URL);
  await BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    NEW_URL
  );
  // Ensure we've invalidated the actor
  await TestUtils.waitForCondition(
    () => wgp.isClosed,
    "Waiting for actor to be dead after onunload"
  );
  contextMenu.hidePopup();

  // Check the menu *still* looks the same (and keep it open again):
  await test_contextmenu("#input_spellcheck_correct", kCorrectItems, {
    waitForSpellCheck: true,
    keepMenuOpen: true,
  });

  // Check what happens if the actor stays alive by loading the same page
  // again; now the context menu stuff should be destroyed by the menu
  // hiding, nothing else.
  wgp = gBrowser.selectedBrowser.browsingContext.currentWindowGlobal;
  BrowserTestUtils.loadURI(gBrowser.selectedBrowser, NEW_URL);
  await BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    NEW_URL
  );
  contextMenu.hidePopup();

  // Check the menu still looks the same:
  await test_contextmenu("#input_spellcheck_correct", kCorrectItems, {
    waitForSpellCheck: true,
  });
  // And test it a last time without any navigation:
  await test_contextmenu("#input_spellcheck_correct", kCorrectItems, {
    waitForSpellCheck: true,
  });
});

add_task(async function test_cleanup() {
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
