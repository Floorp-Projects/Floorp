/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

// The no-cpows-in-tests check isn't very smart, simply warning if it finds
// a variable named `content`. For Chrome compatibility, the Omnibox API uses
// that name for setting the text of a suggestion, and that's all this test uses
// it for, so we can disable it for this test.
/* eslint-disable mozilla/no-cpows-in-tests */

add_task(async function() {
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

  async function expectEvent(event, expected = {}) {
    let actual = await extension.awaitMessage(event);
    if (expected.text) {
      is(actual.text, expected.text,
         `Expected "${event}" to have fired with text: "${expected.text}".`);
    }
    if (expected.disposition) {
      is(actual.disposition, expected.disposition,
         `Expected "${event}" to have fired with disposition: "${expected.disposition}".`);
    }
  }

  async function waitForAutocompleteResultAt(index) {
    let searchString = gURLBar.controller.searchString;
    await BrowserTestUtils.waitForCondition(
      () => gURLBar.popup.richlistbox.children.length > index &&
            gURLBar.popup.richlistbox.children[index].getAttribute("ac-text") == searchString,
      `Waiting for the autocomplete result for "${searchString}" at [${index}] to appear`);
    // Ensure the addition is complete, for proper mouse events on the entries.
    await new Promise(resolve => window.requestIdleCallback(resolve, {timeout: 1000}));
    return gURLBar.popup.richlistbox.children[index];
  }

  async function promiseClickOnItem(item, details) {
    // The Address Bar panel is animated and updated on a timer, thus it may not
    // yet be listening to events when we try to click on it.  This uses a
    // polling strategy to repeat the click, if it doesn't go through.
    let clicked = false;
    item.addEventListener("mousedown", () => { clicked = true; }, {once: true});
    while (!clicked) {
      EventUtils.synthesizeMouseAtCenter(item, details);
      await new Promise(r => window.requestIdleCallback(r, {timeout: 1000}));
    }
  }

  let inputSessionSerial = 0;
  async function startInputSession(indexToWaitFor) {
    gURLBar.focus();
    gURLBar.value = keyword;
    EventUtils.synthesizeKey(" ", {});
    await expectEvent("on-input-started-fired");
    // Always use a different input at every invokation, so that
    // waitForAutocompleteResultAt can distinguish different cases.
    let char = ((inputSessionSerial++) % 10).toString();
    EventUtils.synthesizeKey(char, {});

    await expectEvent("on-input-changed-fired", {text: char});
    // Wait for the autocomplete search. Note that we cannot wait for the search
    // to be complete, since the add-on doesn't communicate when it's done, so
    // just check matches count.
    await waitForAutocompleteResultAt(indexToWaitFor);

    return char;
  }

  async function testInputEvents() {
    gURLBar.focus();

    // Start an input session by typing in <keyword><space>.
    for (let letter of keyword) {
      EventUtils.synthesizeKey(letter, {});
    }
    EventUtils.synthesizeKey(" ", {});
    await expectEvent("on-input-started-fired");

    // Test canceling the input before any changed events fire.
    EventUtils.synthesizeKey("VK_BACK_SPACE", {});
    await expectEvent("on-input-cancelled-fired");

    EventUtils.synthesizeKey(" ", {});
    await expectEvent("on-input-started-fired");

    // Test submitting the input before any changed events fire.
    EventUtils.synthesizeKey("VK_RETURN", {});
    await expectEvent("on-input-entered-fired");

    gURLBar.focus();

    // Start an input session by typing in <keyword><space>.
    for (let letter of keyword) {
      EventUtils.synthesizeKey(letter, {});
    }
    EventUtils.synthesizeKey(" ", {});
    await expectEvent("on-input-started-fired");

    // We should expect input changed events now that the keyword is active.
    EventUtils.synthesizeKey("b", {});
    await expectEvent("on-input-changed-fired", {text: "b"});

    EventUtils.synthesizeKey("c", {});
    await expectEvent("on-input-changed-fired", {text: "bc"});

    EventUtils.synthesizeKey("VK_BACK_SPACE", {});
    await expectEvent("on-input-changed-fired", {text: "b"});

    // Even though the input is <keyword><space> We should not expect an
    // input started event to fire since the keyword is active.
    EventUtils.synthesizeKey("VK_BACK_SPACE", {});
    await expectEvent("on-input-changed-fired", {text: ""});

    // Make the keyword inactive by hitting backspace.
    EventUtils.synthesizeKey("VK_BACK_SPACE", {});
    await expectEvent("on-input-cancelled-fired");

    // Activate the keyword by typing a space.
    // Expect onInputStarted to fire.
    EventUtils.synthesizeKey(" ", {});
    await expectEvent("on-input-started-fired");

    // onInputChanged should fire even if a space is entered.
    EventUtils.synthesizeKey(" ", {});
    await expectEvent("on-input-changed-fired", {text: " "});

    // The active session should cancel if the input blurs.
    gURLBar.blur();
    await expectEvent("on-input-cancelled-fired");
  }

  async function testHeuristicResult(expectedText, setDefaultSuggestion) {
    if (setDefaultSuggestion) {
      extension.sendMessage("set-default-suggestion", {
        suggestion: {
          description: expectedText,
        },
      });
      await extension.awaitMessage("default-suggestion-set");
    }

    let text = await startInputSession(0);

    let item = gURLBar.popup.richlistbox.children[0];

    is(item.getAttribute("title"), expectedText,
       `Expected heuristic result to have title: "${expectedText}".`);

    is(item.getAttribute("displayurl"), `${keyword} ${text}`,
       `Expected heuristic result to have displayurl: "${keyword} ${text}".`);

    let promiseEvent = expectEvent("on-input-entered-fired", {
      text,
      disposition: "currentTab",
    });
    await promiseClickOnItem(item, {});
    await promiseEvent;
  }

  async function testDisposition(suggestionIndex, expectedDisposition, expectedText) {
    await startInputSession(suggestionIndex);

    // Select the suggestion.
    for (let i = 0; i < suggestionIndex; i++) {
      EventUtils.synthesizeKey("VK_DOWN", {});
    }

    let promiseEvent = expectEvent("on-input-entered-fired", {
      text: expectedText,
      disposition: expectedDisposition,
    });

    let item = gURLBar.popup.richlistbox.children[suggestionIndex];
    if (expectedDisposition == "currentTab") {
      await promiseClickOnItem(item, {});
    } else if (expectedDisposition == "newForegroundTab") {
      await promiseClickOnItem(item, {accelKey: true});
    } else if (expectedDisposition == "newBackgroundTab") {
      await promiseClickOnItem(item, {shiftKey: true, accelKey: true});
    }
    await promiseEvent;
  }

  async function testSuggestions(info) {
    extension.sendMessage("set-synchronous", {synchronous: false});

    function expectSuggestion({content, description}, index) {
      let item = gURLBar.popup.richlistbox.children[index + 1]; // Skip the heuristic result.

      ok(!!item, "Expected item to exist");
      is(item.getAttribute("title"), description,
         `Expected suggestion to have title: "${description}".`);

      is(item.getAttribute("displayurl"), `${keyword} ${content}`,
         `Expected suggestion to have displayurl: "${keyword} ${content}".`);
    }

    let text = await startInputSession(info.suggestions.length - 1);

    extension.sendMessage(info.test);
    await extension.awaitMessage("test-ready");

    info.suggestions.forEach(expectSuggestion);

    let promiseEvent = expectEvent("on-input-entered-fired", {
      text,
      disposition: "currentTab",
    });
    await promiseClickOnItem(gURLBar.popup.richlistbox.children[0], {});
    await promiseEvent;
  }

  await extension.startup();

  await SimpleTest.promiseFocus(window);

  await testInputEvents();

  // Test the heuristic result with default suggestions.
  await testHeuristicResult("Generated extension", false /* setDefaultSuggestion */);
  await testHeuristicResult("hello world", true /* setDefaultSuggestion */);
  await testHeuristicResult("foo bar", true /* setDefaultSuggestion */);

  let suggestions = [
    {content: "a", description: "select a"},
    {content: "b", description: "select b"},
    {content: "c", description: "select c"},
  ];

  extension.sendMessage("set-suggestions", {suggestions});
  await extension.awaitMessage("suggestions-set");

  // Test each suggestion and search disposition.
  await testDisposition(1, "currentTab", suggestions[0].content);
  await testDisposition(2, "newForegroundTab", suggestions[1].content);
  await testDisposition(3, "newBackgroundTab", suggestions[2].content);

  extension.sendMessage("set-suggestions", {suggestions});
  await extension.awaitMessage("suggestions-set");

  // Test adding suggestions asynchronously.
  await testSuggestions({
    test: "test-multiple-suggest-calls",
    suggestions,
  });
  await testSuggestions({
    test: "test-suggestions-after-delay",
    suggestions,
  });

  // Start monitoring the console.
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

  await extension2.startup();

  // Stop monitoring the console and confirm the correct errors are logged.
  SimpleTest.endMonitorConsole();
  await waitForConsole;

  await extension2.unload();
  await extension.unload();
});
