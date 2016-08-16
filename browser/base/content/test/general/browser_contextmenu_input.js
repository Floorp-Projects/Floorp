"use strict";

let contextMenu;
let hasPocket = Services.prefs.getBoolPref("extensions.pocket.enabled");

add_task(function* test_setup() {
  const example_base = "http://example.com/browser/browser/base/content/test/general/";
  const url = example_base + "subtst_contextmenu_input.html";
  yield BrowserTestUtils.openNewForegroundTab(gBrowser, url);

  const chrome_base = "chrome://mochitests/content/browser/browser/base/content/test/general/";
  const contextmenu_common = chrome_base + "contextmenu_common.js";
  Services.scriptloader.loadSubScript(contextmenu_common, this);
});

add_task(function* test_text_input() {
  yield test_contextmenu("#input_text",
    ["context-undo",        false,
     "---",                 null,
     "context-cut",         true,
     "context-copy",        true,
     "context-paste",       null, // ignore clipboard state
     "context-delete",      false,
     "---",                 null,
     "context-selectall",   false,
     "---",                 null,
     "spell-check-enabled", true]);
});

add_task(function* test_text_input_spellcheck() {
  yield test_contextmenu("#input_spellcheck_no_value",
    ["context-undo",        false,
     "---",                 null,
     "context-cut",         true,
     "context-copy",        true,
     "context-paste",       null, // ignore clipboard state
     "context-delete",      false,
     "---",                 null,
     "context-selectall",   false,
     "---",                 null,
     "spell-check-enabled", true,
     "spell-dictionaries",  true,
         ["spell-check-dictionary-en-US", true,
          "---",                          null,
          "spell-add-dictionaries",       true], null],
    {
      waitForSpellCheck: true,
      // Need to dynamically add/remove the "password" type or LoginManager
      // will think that the form inputs on the page are part of a login
      // and will add fill-login context menu items.
      *preCheckContextMenuFn() {
        yield ContentTask.spawn(gBrowser.selectedBrowser, null, function*() {
          let doc = content.document;
          let input = doc.getElementById("input_spellcheck_no_value");
          input.setAttribute("spellcheck", "true");
          input.clientTop; // force layout flush
        });
      },
    }
  );
});

add_task(function* test_text_input_spellcheckwrong() {
  yield test_contextmenu("#input_spellcheck_incorrect",
    ["*prodigality",        true, // spelling suggestion
     "spell-add-to-dictionary", true,
     "---",                 null,
     "context-undo",        false,
     "---",                 null,
     "context-cut",         true,
     "context-copy",        true,
     "context-paste",       null, // ignore clipboard state
     "context-delete",      false,
     "---",                 null,
     "context-selectall",   true,
     "---",                 null,
     "spell-check-enabled", true,
     "spell-dictionaries",  true,
         ["spell-check-dictionary-en-US", true,
          "---",                          null,
          "spell-add-dictionaries",       true], null],
     {waitForSpellCheck: true}
   );
});

add_task(function* test_text_input_spellcheckcorrect() {
  yield test_contextmenu("#input_spellcheck_correct",
    ["context-undo",        false,
     "---",                 null,
     "context-cut",         true,
     "context-copy",        true,
     "context-paste",       null, // ignore clipboard state
     "context-delete",      false,
     "---",                 null,
     "context-selectall",   true,
     "---",                 null,
     "spell-check-enabled", true,
     "spell-dictionaries",  true,
         ["spell-check-dictionary-en-US", true,
          "---",                          null,
          "spell-add-dictionaries",       true], null],
     {waitForSpellCheck: true}
  );
});

add_task(function* test_text_input_disabled() {
  yield test_contextmenu("#input_disabled",
    ["context-undo",        false,
     "---",                 null,
     "context-cut",         true,
     "context-copy",        true,
     "context-paste",       null, // ignore clipboard state
     "context-delete",      false,
     "---",                 null,
     "context-selectall",   true,
     "---",                 null,
     "spell-check-enabled", true],
    {skipFocusChange: true}
  );
});

add_task(function* test_password_input() {
  todo(false, "context-selectall is enabled on osx-e10s, and windows when" +
              " it should be disabled");
  yield test_contextmenu("#input_password",
    ["context-undo",        false,
     "---",                 null,
     "context-cut",         true,
     "context-copy",        true,
     "context-paste",       null, // ignore clipboard state
     "context-delete",      false,
     "---",                 null,
     "context-selectall",   null,
     "---",                 null,
     "fill-login",          null,
       ["fill-login-no-logins",       false,
        "---",                        null,
        "fill-login-saved-passwords", true], null],
    {
      skipFocusChange: true,
      // Need to dynamically add/remove the "password" type or LoginManager
      // will think that the form inputs on the page are part of a login
      // and will add fill-login context menu items.
      *preCheckContextMenuFn() {
        yield ContentTask.spawn(gBrowser.selectedBrowser, null, function*() {
          let doc = content.document;
          let input = doc.getElementById("input_password");
          input.type = "password";
          input.clientTop; // force layout flush
        });
      },
      *postCheckContextMenuFn() {
        yield ContentTask.spawn(gBrowser.selectedBrowser, null, function*() {
          let doc = content.document;
          let input = doc.getElementById("input_password");
          input.type = "text";
          input.clientTop; // force layout flush
        });
      },
    }
  );
});

add_task(function* test_tel_email_url_number_input() {
  todo(false, "context-selectall is enabled on osx-e10s, and windows when" +
              " it should be disabled");
  for (let selector of ["#input_email", "#input_url", "#input_tel", "#input_number"]) {
    yield test_contextmenu(selector,
      ["context-undo",        false,
       "---",                 null,
       "context-cut",         true,
       "context-copy",        true,
       "context-paste",       null, // ignore clipboard state
       "context-delete",      false,
       "---",                 null,
       "context-selectall",   null],
      {skipFocusChange: true}
    );
  }
});

add_task(function* test_date_time_color_range_month_week_input() {
  for (let selector of ["#input_date", "#input_time", "#input_color",
                        "#input_range", "#input_month"]) {
    yield test_contextmenu(selector,
      ["context-navigation", null,
           ["context-back",         false,
            "context-forward",      false,
            "context-reload",       true,
            "context-bookmarkpage", true], null,
       "---",                  null,
       "context-savepage",     true,
       ...(hasPocket ? ["context-pocket", true] : []),
       "---",                  null,
       "context-viewbgimage",  false,
       "context-selectall",    null,
       "---",                  null,
       "context-viewsource",   true,
       "context-viewinfo",     true],
      {skipFocusChange: true}
    );
  }
});

add_task(function* test_search_input() {
  todo(false, "context-selectall is enabled on osx-e10s, and windows when" +
              " it should be disabled");
  yield test_contextmenu("#input_search",
    ["context-undo",        false,
     "---",                 null,
     "context-cut",         true,
     "context-copy",        true,
     "context-paste",       null, // ignore clipboard state
     "context-delete",      false,
     "---",                 null,
     "context-selectall",   null,
     "---",                 null,
     "spell-check-enabled", true],
    {skipFocusChange: true}
  );
});

add_task(function* test_datetime_datetimelocal_input_todos() {
  for (let type of ["datetime", "datetime-local"]) {
    let returnedType = yield ContentTask.spawn(gBrowser.selectedBrowser, type, function*(type) {
      let doc = content.document;
      let input = doc.getElementById("input_" + type);
      return input.type;
    });
    todo_is(returnedType, type, `TODO: add test for ${type} input fields`);
  }
});

add_task(function* test_text_input_readonly() {
  todo(false, "context-selectall is enabled on osx-e10s, and windows when" +
              " it should be disabled");
  todo(false, "spell-check should not be enabled for input[readonly]. see bug 1246296");
  yield test_contextmenu("#input_readonly",
    ["context-undo",        false,
     "---",                 null,
     "context-cut",         true,
     "context-copy",        true,
     "context-paste",       null, // ignore clipboard state
     "context-delete",      false,
     "---",                 null,
     "context-selectall",   null],
    {skipFocusChange: true}
  );
});

add_task(function* test_cleanup() {
  yield BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
