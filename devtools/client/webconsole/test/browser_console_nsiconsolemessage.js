/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Check that nsIConsoleMessages are displayed in the Browser Console.
// See bug 859756.

"use strict";

const TEST_URI = "data:text/html;charset=utf8,<title>bug859756</title>\n" +
                 "<p>hello world\n<p>nsIConsoleMessages ftw!";

function test() {
  const FILTER_PREF = "devtools.browserconsole.filter.jslog";
  Services.prefs.setBoolPref(FILTER_PREF, true);

  registerCleanupFunction(() => {
    Services.prefs.clearUserPref(FILTER_PREF);
  });

  Task.spawn(function*() {
    const {tab} = yield loadTab(TEST_URI);

    // Test for cached nsIConsoleMessages.
    Services.console.logStringMessage("test1 for bug859756");

    info("open web console");
    let hud = yield openConsole(tab);

    ok(hud, "web console opened");
    Services.console.logStringMessage("do-not-show-me");
    content.console.log("foobarz");

    yield waitForMessages({
      webconsole: hud,
      messages: [{
        text: "foobarz",
        category: CATEGORY_WEBDEV,
        severity: SEVERITY_LOG,
      }],
    });

    let text = hud.outputNode.textContent;
    is(text.indexOf("do-not-show-me"), -1,
       "nsIConsoleMessages are not displayed");
    is(text.indexOf("test1 for bug859756"), -1,
       "nsIConsoleMessages are not displayed (confirmed)");

    yield closeConsole(tab);

    info("web console closed");
    hud = yield HUDService.toggleBrowserConsole();
    ok(hud, "browser console opened");

    Services.console.logStringMessage("test2 for bug859756");

    let results = yield waitForMessages({
      webconsole: hud,
      messages: [{
        text: "test1 for bug859756",
        category: CATEGORY_JS,
      }, {
        text: "test2 for bug859756",
        category: CATEGORY_JS,
      }, {
        text: "do-not-show-me",
        category: CATEGORY_JS,
      }],
    });

    let msg = [...results[2].matched][0];
    ok(msg, "message element for do-not-show-me (nsIConsoleMessage)");
    isnot(msg.textContent.indexOf("do-not-show"), -1,
          "element content is correct");
    ok(!msg.classList.contains("filtered-by-type"), "element is not filtered");

    hud.setFilterState("jslog", false);

    ok(msg.classList.contains("filtered-by-type"), "element is filtered");
  }).then(finishTest);
}
