/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that autocomplete doesn't break when trying to reach into objects from
// a different domain, bug 989025.

"use strict";

function test() {
  let hud;

  const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/" +
                   "test/test-bug-989025-iframe-parent.html";

  Task.spawn(function*() {
    const {tab} = yield loadTab(TEST_URI);
    hud = yield openConsole(tab);

    hud.jsterm.execute("document.title");

    yield waitForMessages({
      webconsole: hud,
      messages: [{
        text: "989025 - iframe parent",
        category: CATEGORY_OUTPUT,
      }],
    });

    let autocompleteUpdated = hud.jsterm.once("autocomplete-updated");

    hud.jsterm.setInputValue("window[0].document");
    executeSoon(() => {
      EventUtils.synthesizeKey(".", {});
    });

    yield autocompleteUpdated;

    hud.jsterm.setInputValue("window[0].document.title");
    EventUtils.synthesizeKey("VK_RETURN", {});

    yield waitForMessages({
      webconsole: hud,
      messages: [{
        text: "Permission denied",
        category: CATEGORY_OUTPUT,
        severity: SEVERITY_ERROR,
      }],
    });

    hud.jsterm.execute("window.location");

    yield waitForMessages({
      webconsole: hud,
      messages: [{
        text: "test-bug-989025-iframe-parent.html",
        category: CATEGORY_OUTPUT,
      }],
    });

    yield closeConsole(tab);
  }).then(finishTest);
}
