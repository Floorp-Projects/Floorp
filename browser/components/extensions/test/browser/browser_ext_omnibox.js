/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const { UrlbarTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/UrlbarTestUtils.sys.mjs"
);

const keyword = "VeryUniqueKeywordThatDoesNeverMatchAnyTestUrl";

add_task(async function() {
  // This keyword needs to be unique to prevent history entries from unrelated
  // tests from appearing in the suggestions list.
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      omnibox: {
        keyword: keyword,
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
        browser.test.sendMessage("on-input-changed-fired", { text });
      });

      browser.omnibox.onInputCancelled.addListener(() => {
        browser.test.sendMessage("on-input-cancelled-fired");
      });

      browser.omnibox.onInputEntered.addListener((text, disposition) => {
        browser.test.sendMessage("on-input-entered-fired", {
          text,
          disposition,
        });
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
            browser.test.sendMessage("set-synchronous-set");
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
      is(
        actual.text,
        expected.text,
        `Expected "${event}" to have fired with text: "${expected.text}".`
      );
    }
    if (expected.disposition) {
      is(
        actual.disposition,
        expected.disposition,
        `Expected "${event}" to have fired with disposition: "${expected.disposition}".`
      );
    }
  }

  async function waitForResult(index, searchString) {
    let result = await UrlbarTestUtils.getDetailsOfResultAt(window, index);
    // Ensure the addition is complete, for proper mouse events on the entries.
    await new Promise(resolve =>
      window.requestIdleCallback(resolve, { timeout: 1000 })
    );
    return result;
  }

  async function promiseClickOnItem(index, details) {
    // The Address Bar panel is animated and updated on a timer, thus it may not
    // yet be listening to events when we try to click on it.  This uses a
    // polling strategy to repeat the click, if it doesn't go through.
    let clicked = false;
    let element = await UrlbarTestUtils.waitForAutocompleteResultAt(
      window,
      index
    );
    element.addEventListener(
      "mousedown",
      () => {
        clicked = true;
      },
      { once: true }
    );
    while (!clicked) {
      EventUtils.synthesizeMouseAtCenter(element, details);
      await new Promise(r => window.requestIdleCallback(r, { timeout: 1000 }));
    }
  }

  let inputSessionSerial = 0;
  async function startInputSession() {
    gURLBar.focus();
    gURLBar.value = keyword;
    EventUtils.sendString(" ");
    await expectEvent("on-input-started-fired");
    // Always use a different input at every invokation, so that
    // waitForResult can distinguish different cases.
    let char = (inputSessionSerial++ % 10).toString();
    EventUtils.sendString(char);

    await expectEvent("on-input-changed-fired", { text: char });
    return char;
  }

  async function testInputEvents() {
    gURLBar.focus();

    // Start an input session by typing in <keyword><space>.
    EventUtils.sendString(keyword + " ");
    await expectEvent("on-input-started-fired");

    // Test canceling the input before any changed events fire.
    EventUtils.synthesizeKey("KEY_Backspace");
    await expectEvent("on-input-cancelled-fired");

    EventUtils.sendString(" ");
    await expectEvent("on-input-started-fired");

    // Test submitting the input before any changed events fire.
    EventUtils.synthesizeKey("KEY_Enter");
    await expectEvent("on-input-entered-fired");

    gURLBar.focus();

    // Start an input session by typing in <keyword><space>.
    EventUtils.sendString(keyword + " ");
    await expectEvent("on-input-started-fired");

    // We should expect input changed events now that the keyword is active.
    EventUtils.sendString("b");
    await expectEvent("on-input-changed-fired", { text: "b" });

    EventUtils.sendString("c");
    await expectEvent("on-input-changed-fired", { text: "bc" });

    EventUtils.synthesizeKey("KEY_Backspace");
    await expectEvent("on-input-changed-fired", { text: "b" });

    // Even though the input is <keyword><space> We should not expect an
    // input started event to fire since the keyword is active.
    EventUtils.synthesizeKey("KEY_Backspace");
    await expectEvent("on-input-changed-fired", { text: "" });

    // Make the keyword inactive by hitting backspace.
    EventUtils.synthesizeKey("KEY_Backspace");
    await expectEvent("on-input-cancelled-fired");

    // Activate the keyword by typing a space.
    // Expect onInputStarted to fire.
    EventUtils.sendString(" ");
    await expectEvent("on-input-started-fired");

    // onInputChanged should fire even if a space is entered.
    EventUtils.sendString(" ");
    await expectEvent("on-input-changed-fired", { text: " " });

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

    let text = await startInputSession();
    let result = await waitForResult(0);

    Assert.equal(
      result.displayed.title,
      expectedText,
      `Expected heuristic result to have title: "${expectedText}".`
    );

    Assert.equal(
      result.displayed.action,
      `${keyword} ${text}`,
      `Expected heuristic result to have displayurl: "${keyword} ${text}".`
    );

    let promiseEvent = expectEvent("on-input-entered-fired", {
      text,
      disposition: "currentTab",
    });
    await promiseClickOnItem(0, {});
    await promiseEvent;
  }

  async function testDisposition(
    suggestionIndex,
    expectedDisposition,
    expectedText
  ) {
    await startInputSession();
    await waitForResult(suggestionIndex);

    // Select the suggestion.
    EventUtils.synthesizeKey("KEY_ArrowDown", { repeat: suggestionIndex });

    let promiseEvent = expectEvent("on-input-entered-fired", {
      text: expectedText,
      disposition: expectedDisposition,
    });

    if (expectedDisposition == "currentTab") {
      await promiseClickOnItem(suggestionIndex, {});
    } else if (expectedDisposition == "newForegroundTab") {
      await promiseClickOnItem(suggestionIndex, { accelKey: true });
    } else if (expectedDisposition == "newBackgroundTab") {
      await promiseClickOnItem(suggestionIndex, {
        shiftKey: true,
        accelKey: true,
      });
    }
    await promiseEvent;
  }

  async function testSuggestions(info) {
    extension.sendMessage("set-synchronous", { synchronous: false });
    await extension.awaitMessage("set-synchronous-set");

    let text = await startInputSession();

    extension.sendMessage(info.test);
    await extension.awaitMessage("test-ready");

    await waitForResult(info.suggestions.length - 1);
    // Skip the heuristic result.
    let index = 1;
    for (let { content, description } of info.suggestions) {
      let item = await UrlbarTestUtils.getDetailsOfResultAt(window, index);
      Assert.equal(
        item.displayed.title,
        description,
        `Expected suggestion to have title: "${description}".`
      );
      Assert.equal(
        item.displayed.action,
        `${keyword} ${content}`,
        `Expected suggestion to have displayurl: "${keyword} ${content}".`
      );
      index++;
    }

    let promiseEvent = expectEvent("on-input-entered-fired", {
      text,
      disposition: "currentTab",
    });
    await promiseClickOnItem(0, {});
    await promiseEvent;
  }

  await extension.startup();

  await SimpleTest.promiseFocus(window);

  await testInputEvents();

  // Test the heuristic result with default suggestions.
  await testHeuristicResult(
    "Generated extension",
    false /* setDefaultSuggestion */
  );
  await testHeuristicResult("hello world", true /* setDefaultSuggestion */);
  await testHeuristicResult("foo bar", true /* setDefaultSuggestion */);

  let suggestions = [
    { content: "a", description: "select a" },
    { content: "b", description: "select b" },
    { content: "c", description: "select c" },
  ];

  extension.sendMessage("set-suggestions", { suggestions });
  await extension.awaitMessage("suggestions-set");

  // Test each suggestion and search disposition.
  await testDisposition(1, "currentTab", suggestions[0].content);
  await testDisposition(2, "newForegroundTab", suggestions[1].content);
  await testDisposition(3, "newBackgroundTab", suggestions[2].content);

  extension.sendMessage("set-suggestions", { suggestions });
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

  // When we're the first task to be added, `waitForExplicitFinish()` may not have
  // been called yet. Let's just do that, otherwise the `monitorConsole` will make
  // the test fail with a failing assertion.
  SimpleTest.waitForExplicitFinish();
  // Start monitoring the console.
  let waitForConsole = new Promise(resolve => {
    SimpleTest.monitorConsole(resolve, [
      {
        message: new RegExp(
          `The keyword provided is already registered: "${keyword}"`
        ),
      },
    ]);
  });

  // Try registering another extension with the same keyword
  let extension2 = ExtensionTestUtils.loadExtension({
    manifest: {
      omnibox: {
        keyword: keyword,
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

add_task(async function test_omnibox_event_page() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.eventPages.enabled", true]],
  });

  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest: {
      applications: { gecko: { id: "eventpage@omnibox" } },
      omnibox: {
        keyword: keyword,
      },
      background: { persistent: false },
    },
    background() {
      browser.omnibox.onInputStarted.addListener(() => {
        browser.test.sendMessage("onInputStarted");
      });
      browser.omnibox.onInputEntered.addListener(() => {});
      browser.omnibox.onInputChanged.addListener(() => {});
      browser.omnibox.onInputCancelled.addListener(() => {});
      browser.test.sendMessage("ready");
    },
  });

  const EVENTS = [
    "onInputStarted",
    "onInputEntered",
    "onInputChanged",
    "onInputCancelled",
  ];

  await extension.startup();
  await extension.awaitMessage("ready");
  for (let event of EVENTS) {
    assertPersistentListeners(extension, "omnibox", event, {
      primed: false,
    });
  }

  // test events waken background
  await extension.terminateBackground();
  for (let event of EVENTS) {
    assertPersistentListeners(extension, "omnibox", event, {
      primed: true,
    });
  }

  // Activate the keyword by typing a space.
  // Expect onInputStarted to fire.
  gURLBar.focus();
  gURLBar.value = keyword;
  EventUtils.sendString(" ");

  await extension.awaitMessage("ready");
  await extension.awaitMessage("onInputStarted");
  ok(true, "persistent event woke background");
  for (let event of EVENTS) {
    assertPersistentListeners(extension, "omnibox", event, {
      primed: false,
    });
  }

  await extension.unload();
  await SpecialPowers.popPrefEnv();
});
