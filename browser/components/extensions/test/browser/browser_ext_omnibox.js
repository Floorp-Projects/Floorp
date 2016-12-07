/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

// The no-cpows-in-tests check isn't very smart, simply warning if it finds
// a variable named `content`. For Chrome compatibility, the Omnibox API uses
// that name for setting the text of a suggestion, and that's all this test uses
// it for, so we can disable it for this test.
/* eslint-disable mozilla/no-cpows-in-tests */

function* setup() {
  const SUGGEST_URLBAR_PREF = "browser.urlbar.suggest.searches";
  Services.prefs.setBoolPref(SUGGEST_URLBAR_PREF, false);

  registerCleanupFunction(() => {
    Services.prefs.clearUserPref(SUGGEST_URLBAR_PREF);
  });
}

add_task(function* () {
  let keyword = "test";

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "omnibox": {
        "keyword": keyword,
      },
    },

    background: function() {
      browser.omnibox.onInputStarted.addListener(() => {
        browser.test.sendMessage("on-input-started-fired");
      });

      let synchronous = true;
      let suggestions = null;
      let suggestCallback = null;

      browser.omnibox.onInputChanged.addListener((text, suggest) => {
        if (synchronous && suggestions) {
          suggest(suggestions);
        } else {
          suggestCallback = suggest;
        }
        browser.test.sendMessage("on-input-changed-fired", {text});
      });

      browser.omnibox.onInputCancelled.addListener(() => {
        browser.test.sendMessage("on-input-cancelled-fired");
      });

      browser.omnibox.onInputEntered.addListener((text, disposition) => {
        browser.test.sendMessage("on-input-entered-fired", {text, disposition});
      });

      browser.test.onMessage.addListener((msg, data) => {
        switch (msg) {
          case "set-suggestions":
            suggestions = data.suggestions;
            browser.test.sendMessage("suggestions-set");
            break;
          case "set-default-suggestion":
            browser.omnibox.setDefaultSuggestion(data.suggestion);
            browser.test.sendMessage("default-suggestion-set");
            break;
          case "set-synchronous":
            synchronous = data.synchronous;
            break;
          case "test-multiple-suggest-calls":
            suggestions.forEach(suggestion => suggestCallback([suggestion]));
            browser.test.sendMessage("test-ready");
            break;
          case "test-suggestions-after-delay":
            Promise.resolve().then(() => {
              suggestCallback(suggestions);
              browser.test.sendMessage("test-ready");
            });
            break;
        }
      });
    },
  });

  function* expectEvent(event, expected = {}) {
    let actual = yield extension.awaitMessage(event);
    if (expected.text) {
      is(actual.text, expected.text,
        `Expected "${event}" to have fired with text: "${expected.text}".`);
    }
    if (expected.disposition) {
      is(actual.disposition, expected.disposition,
        `Expected "${event}" to have fired with disposition: "${expected.disposition}".`);
    }
  }

  function* startInputSession() {
    gURLBar.focus();
    gURLBar.value = keyword;
    EventUtils.synthesizeKey(" ", {});
    yield expectEvent("on-input-started-fired");
    EventUtils.synthesizeKey("t", {});
    yield expectEvent("on-input-changed-fired", {text: "t"});
    return "t";
  }

  function* testInputEvents() {
    gURLBar.focus();

    // Start an input session by typing in <keyword><space>.
    for (let letter of keyword) {
      EventUtils.synthesizeKey(letter, {});
    }
    EventUtils.synthesizeKey(" ", {});
    yield expectEvent("on-input-started-fired");

    // We should expect input changed events now that the keyword is active.
    EventUtils.synthesizeKey("b", {});
    yield expectEvent("on-input-changed-fired", {text: "b"});

    EventUtils.synthesizeKey("c", {});
    yield expectEvent("on-input-changed-fired", {text: "bc"});

    EventUtils.synthesizeKey("VK_BACK_SPACE", {});
    yield expectEvent("on-input-changed-fired", {text: "b"});

    // Even though the input is <keyword><space> We should not expect an
    // input started event to fire since the keyword is active.
    EventUtils.synthesizeKey("VK_BACK_SPACE", {});
    yield expectEvent("on-input-changed-fired", {text: ""});

    // Make the keyword inactive by hitting backspace.
    EventUtils.synthesizeKey("VK_BACK_SPACE", {});
    yield expectEvent("on-input-cancelled-fired");

    // Activate the keyword by typing a space.
    // Expect onInputStarted to fire.
    EventUtils.synthesizeKey(" ", {});
    yield expectEvent("on-input-started-fired");

    // onInputChanged should fire even if a space is entered.
    EventUtils.synthesizeKey(" ", {});
    yield expectEvent("on-input-changed-fired", {text: " "});

    // The active session should cancel if the input blurs.
    gURLBar.blur();
    yield expectEvent("on-input-cancelled-fired");
  }

  function* testHeuristicResult(expectedText, setDefaultSuggestion) {
    if (setDefaultSuggestion) {
      extension.sendMessage("set-default-suggestion", {
        suggestion: {
          description: expectedText,
        },
      });
      yield extension.awaitMessage("default-suggestion-set");
    }

    let text = yield startInputSession();

    let item = gURLBar.popup.richlistbox.children[0];

    is(item.getAttribute("title"), expectedText,
      `Expected heuristic result to have title: "${expectedText}".`);

    is(item.getAttribute("displayurl"), `${keyword} ${text}`,
      `Expected heuristic result to have displayurl: "${keyword} ${text}".`);

    EventUtils.synthesizeMouseAtCenter(item, {});

    yield expectEvent("on-input-entered-fired", {
      text,
      disposition: "currentTab",
    });
  }

  function* testDisposition(suggestionIndex, expectedDisposition, expectedText) {
    yield startInputSession();

    // Select the suggestion.
    for (let i = 0; i < suggestionIndex; i++) {
      EventUtils.synthesizeKey("VK_DOWN", {});
    }

    let item = gURLBar.popup.richlistbox.children[suggestionIndex];
    if (expectedDisposition == "currentTab") {
      EventUtils.synthesizeMouseAtCenter(item, {});
    } else if (expectedDisposition == "newForegroundTab") {
      EventUtils.synthesizeMouseAtCenter(item, {accelKey: true});
    } else if (expectedDisposition == "newBackgroundTab") {
      EventUtils.synthesizeMouseAtCenter(item, {shiftKey: true, accelKey: true});
    }

    yield expectEvent("on-input-entered-fired", {
      text: expectedText,
      disposition: expectedDisposition,
    });
  }

  function* testSuggestions(info) {
    extension.sendMessage("set-synchronous", {synchronous: false});

    function expectSuggestion({content, description}, index) {
      let item = gURLBar.popup.richlistbox.children[index + 1]; // Skip the heuristic result.

      ok(!!item, "Expected item to exist");
      is(item.getAttribute("title"), description,
        `Expected suggestion to have title: "${description}".`);

      is(item.getAttribute("displayurl"), `${keyword} ${content}`,
        `Expected suggestion to have displayurl: "${keyword} ${content}".`);
    }

    let text = yield startInputSession();

    extension.sendMessage(info.test);
    yield extension.awaitMessage("test-ready");

    info.suggestions.forEach(expectSuggestion);

    EventUtils.synthesizeMouseAtCenter(gURLBar.popup.richlistbox.children[0], {});
    yield expectEvent("on-input-entered-fired", {
      text,
      disposition: "currentTab",
    });
  }

  yield setup();
  yield extension.startup();

  yield SimpleTest.promiseFocus(window);

  yield testInputEvents();

  // Test the heuristic result with default suggestions.
  yield testHeuristicResult("Generated extension", false /* setDefaultSuggestion */);
  yield testHeuristicResult("hello world", true /* setDefaultSuggestion */);
  yield testHeuristicResult("foo bar", true /* setDefaultSuggestion */);

  let suggestions = [
    {content: "a", description: "select a"},
    {content: "b", description: "select b"},
    {content: "c", description: "select c"},
  ];

  extension.sendMessage("set-suggestions", {suggestions});
  yield extension.awaitMessage("suggestions-set");

  // Test each suggestion and search disposition.
  yield testDisposition(1, "currentTab", suggestions[0].content);
  yield testDisposition(2, "newForegroundTab", suggestions[1].content);
  yield testDisposition(3, "newBackgroundTab", suggestions[2].content);

  extension.sendMessage("set-suggestions", {suggestions});
  yield extension.awaitMessage("suggestions-set");

  // Test adding suggestions asynchronously.
  yield testSuggestions({
    test: "test-multiple-suggest-calls",
    skipHeuristic: true,
    suggestions,
  });
  yield testSuggestions({
    test: "test-suggestions-after-delay",
    skipHeuristic: true,
    suggestions,
  });

  // Start monitoring the console.
  SimpleTest.waitForExplicitFinish();
  let waitForConsole = new Promise(resolve => {
    SimpleTest.monitorConsole(resolve, [{
      message: new RegExp(`The keyword provided is already registered: "${keyword}"`),
    }]);
  });

  // Try registering another extension with the same keyword
  let extension2 = ExtensionTestUtils.loadExtension({
    manifest: {
      "omnibox": {
        "keyword": keyword,
      },
    },
  });

  yield extension2.startup();

  // Stop monitoring the console and confirm the correct errors are logged.
  SimpleTest.endMonitorConsole();
  yield waitForConsole;

  yield extension2.unload();
  yield extension.unload();
});
