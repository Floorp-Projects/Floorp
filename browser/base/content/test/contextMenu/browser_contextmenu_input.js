"use strict";

let contextMenu;
let hasPocket = Services.prefs.getBoolPref("extensions.pocket.enabled");

add_task(async function test_setup() {
  const example_base =
    "http://example.com/browser/browser/base/content/test/contextMenu/";
  const url = example_base + "subtst_contextmenu_input.html";
  await BrowserTestUtils.openNewForegroundTab(gBrowser, url);

  const chrome_base =
    "chrome://mochitests/content/browser/browser/base/content/test/contextMenu/";
  const contextmenu_common = chrome_base + "contextmenu_common.js";
  /* import-globals-from contextmenu_common.js */
  Services.scriptloader.loadSubScript(contextmenu_common, this);

  // Ensure screenshots is really disabled (bug 1498738)
  const addon = await AddonManager.getAddonByID("screenshots@mozilla.org");
  await addon.disable({ allowSystemAddons: true });
});

add_task(async function test_text_input() {
  await test_contextmenu("#input_text", [
    "context-undo",
    false,
    "---",
    null,
    "context-cut",
    true,
    "context-copy",
    true,
    "context-paste",
    null, // ignore clipboard state
    "context-delete",
    false,
    "---",
    null,
    "context-selectall",
    false,
    "---",
    null,
    "spell-check-enabled",
    true,
  ]);
});

add_task(async function test_text_input_spellcheck() {
  await test_contextmenu(
    "#input_spellcheck_no_value",
    [
      "context-undo",
      false,
      "---",
      null,
      "context-cut",
      true,
      "context-copy",
      true,
      "context-paste",
      null, // ignore clipboard state
      "context-delete",
      false,
      "---",
      null,
      "context-selectall",
      false,
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
      false,
      "---",
      null,
      "context-cut",
      true,
      "context-copy",
      true,
      "context-paste",
      null, // ignore clipboard state
      "context-delete",
      false,
      "---",
      null,
      "context-selectall",
      true,
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

add_task(async function test_text_input_spellcheckcorrect() {
  await test_contextmenu(
    "#input_spellcheck_correct",
    [
      "context-undo",
      false,
      "---",
      null,
      "context-cut",
      true,
      "context-copy",
      true,
      "context-paste",
      null, // ignore clipboard state
      "context-delete",
      false,
      "---",
      null,
      "context-selectall",
      true,
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

add_task(async function test_text_input_disabled() {
  await test_contextmenu(
    "#input_disabled",
    [
      "context-undo",
      false,
      "---",
      null,
      "context-cut",
      true,
      "context-copy",
      true,
      "context-paste",
      null, // ignore clipboard state
      "context-delete",
      false,
      "---",
      null,
      "context-selectall",
      true,
      "---",
      null,
      "spell-check-enabled",
      true,
    ],
    { skipFocusChange: true }
  );
});

add_task(async function test_password_input() {
  await SpecialPowers.pushPrefEnv({
    set: [["signon.generation.enabled", false]],
  });
  todo(
    false,
    "context-selectall is enabled on osx-e10s, and windows when" +
      " it should be disabled"
  );
  await test_contextmenu(
    "#input_password",
    [
      "fill-login",
      null,
      [
        "fill-login-no-logins",
        false,
        "---",
        null,
        "fill-login-saved-passwords",
        true,
      ],
      null,
      "---",
      null,
      "context-undo",
      false,
      "---",
      null,
      "context-cut",
      true,
      "context-copy",
      true,
      "context-paste",
      null, // ignore clipboard state
      "context-delete",
      false,
      "---",
      null,
      "context-selectall",
      null,
    ],
    {
      skipFocusChange: true,
      // Need to dynamically add the "password" type or LoginManager
      // will think that the form inputs on the page are part of a login form
      // and will add fill-login context menu items. The element needs to be
      // re-created as type=text afterwards since it uses hasBeenTypePassword.
      async preCheckContextMenuFn() {
        await SpecialPowers.spawn(
          gBrowser.selectedBrowser,
          [],
          async function() {
            let doc = content.document;
            let input = doc.getElementById("input_password");
            input.type = "password";
            input.clientTop; // force layout flush
          }
        );
      },
      async postCheckContextMenuFn() {
        await SpecialPowers.spawn(
          gBrowser.selectedBrowser,
          [],
          async function() {
            let doc = content.document;
            let input = doc.getElementById("input_password");
            input.outerHTML = `<input id=\"input_password\">`;
            input.clientTop; // force layout flush
          }
        );
      },
    }
  );
  await SpecialPowers.popPrefEnv();
});

add_task(async function test_tel_email_url_number_input() {
  todo(
    false,
    "context-selectall is enabled on osx-e10s, and windows when" +
      " it should be disabled"
  );
  for (let selector of [
    "#input_email",
    "#input_url",
    "#input_tel",
    "#input_number",
  ]) {
    await test_contextmenu(
      selector,
      [
        "context-undo",
        false,
        "---",
        null,
        "context-cut",
        true,
        "context-copy",
        true,
        "context-paste",
        null, // ignore clipboard state
        "context-delete",
        false,
        "---",
        null,
        "context-selectall",
        null,
      ],
      {
        skipFocusChange: true,
      }
    );
  }
});

add_task(
  async function test_date_time_color_range_month_week_datetimelocal_input() {
    for (let selector of [
      "#input_date",
      "#input_time",
      "#input_color",
      "#input_range",
      "#input_month",
      "#input_week",
      "#input_datetime-local",
    ]) {
      await test_contextmenu(
        selector,
        [
          "context-navigation",
          null,
          [
            "context-back",
            false,
            "context-forward",
            false,
            "context-reload",
            true,
            "context-bookmarkpage",
            true,
          ],
          null,
          "---",
          null,
          "context-savepage",
          true,
          ...(hasPocket ? ["context-pocket", true] : []),
          "---",
          null,
          "context-sendpagetodevice",
          null,
          [],
          null,
          "---",
          null,
          "context-viewbgimage",
          false,
          "context-selectall",
          null,
          "---",
          null,
          "context-viewsource",
          true,
          "context-viewinfo",
          true,
        ],
        {
          // XXX Bug 1345081. Currently the Screenshots menu option is shown for
          // various text elements even though it is set to type "page". That bug
          // should remove the need for next line.
          maybeScreenshotsPresent: true,
          skipFocusChange: true,
        }
      );
    }
  }
);

add_task(async function test_search_input() {
  todo(
    false,
    "context-selectall is enabled on osx-e10s, and windows when" +
      " it should be disabled"
  );
  await test_contextmenu(
    "#input_search",
    [
      "context-undo",
      false,
      "---",
      null,
      "context-cut",
      true,
      "context-copy",
      true,
      "context-paste",
      null, // ignore clipboard state
      "context-delete",
      false,
      "---",
      null,
      "context-selectall",
      null,
      "---",
      null,
      "spell-check-enabled",
      true,
    ],
    { skipFocusChange: true }
  );
});

add_task(async function test_text_input_readonly() {
  todo(
    false,
    "context-selectall is enabled on osx-e10s, and windows when" +
      " it should be disabled"
  );
  todo(
    false,
    "spell-check should not be enabled for input[readonly]. see bug 1246296"
  );
  await test_contextmenu(
    "#input_readonly",
    [
      "context-undo",
      false,
      "---",
      null,
      "context-cut",
      true,
      "context-copy",
      true,
      "context-paste",
      null, // ignore clipboard state
      "context-delete",
      false,
      "---",
      null,
      "context-selectall",
      null,
    ],
    {
      // XXX Bug 1345081. Currently the Screenshots menu option is shown for
      // various text elements even though it is set to type "page". That bug
      // should remove the need for next line.
      maybeScreenshotsPresent: true,
      skipFocusChange: true,
    }
  );
});

add_task(async function test_cleanup() {
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
