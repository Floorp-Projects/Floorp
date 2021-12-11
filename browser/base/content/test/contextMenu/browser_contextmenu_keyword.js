/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let contextMenu;

const example_base =
  "http://example.com/browser/browser/base/content/test/contextMenu/";
const MAIN_URL = example_base + "subtst_contextmenu_keyword.html";

add_task(async function test_setup() {
  await BrowserTestUtils.openNewForegroundTab(gBrowser, MAIN_URL);

  const chrome_base =
    "chrome://mochitests/content/browser/browser/base/content/test/contextMenu/";
  const contextmenu_common = chrome_base + "contextmenu_common.js";
  /* import-globals-from contextmenu_common.js */
  Services.scriptloader.loadSubScript(contextmenu_common, this);
});

add_task(async function test_text_input_spellcheck_noform() {
  await test_contextmenu(
    "#input_text_no_form",
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
            let input = doc.getElementById("input_text_no_form");
            input.setAttribute("spellcheck", "true");
            input.clientTop; // force layout flush
          }
        );
      },
    }
  );
});

add_task(async function test_text_input_spellcheck_loginform() {
  await test_contextmenu(
    "#login_text",
    [
      "manage-saved-logins",
      true,
      "---",
      null,
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
            let input = doc.getElementById("login_text");
            input.setAttribute("spellcheck", "true");
            input.clientTop; // force layout flush
          }
        );
      },
    }
  );
});

add_task(async function test_text_input_spellcheck_searchform() {
  await test_contextmenu(
    "#search_text",
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
      "context-keywordfield",
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
            let input = doc.getElementById("search_text");
            input.setAttribute("spellcheck", "true");
            input.clientTop; // force layout flush
          }
        );
      },
    }
  );
});

add_task(async function test_cleanup() {
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
