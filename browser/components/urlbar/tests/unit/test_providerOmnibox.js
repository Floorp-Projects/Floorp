/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim:set ts=2 sw=2 sts=2 et:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { ExtensionSearchHandler } = ChromeUtils.importESModule(
  "resource://gre/modules/ExtensionSearchHandler.sys.mjs"
);

let controller = Cc["@mozilla.org/autocomplete/controller;1"].getService(
  Ci.nsIAutoCompleteController
);

const SUGGEST_PREF = "browser.urlbar.suggest.searches";
const SUGGEST_ENABLED_PREF = "browser.search.suggest.enabled";

async function cleanup() {
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesUtils.history.clear();
}

add_task(function setup() {
  Services.prefs.setBoolPref(SUGGEST_PREF, false);
  Services.prefs.setBoolPref(SUGGEST_ENABLED_PREF, false);

  registerCleanupFunction(async () => {
    Services.prefs.clearUserPref(SUGGEST_PREF);
    Services.prefs.clearUserPref(SUGGEST_ENABLED_PREF);
  });
});

add_task(async function test_correct_errors_are_thrown() {
  let keyword = "foo";
  let anotherKeyword = "bar";
  let unregisteredKeyword = "baz";

  // Register a keyword.
  ExtensionSearchHandler.registerKeyword(keyword, { emit: () => {} });

  // Try registering the keyword again.
  Assert.throws(
    () => ExtensionSearchHandler.registerKeyword(keyword, { emit: () => {} }),
    /The keyword provided is already registered/
  );

  // Register a different keyword.
  ExtensionSearchHandler.registerKeyword(anotherKeyword, { emit: () => {} });

  // Try calling handleSearch for an unregistered keyword.
  let searchData = {
    keyword: unregisteredKeyword,
    text: `${unregisteredKeyword} `,
  };
  Assert.throws(
    () => ExtensionSearchHandler.handleSearch(searchData, () => {}),
    /The keyword provided is not registered/
  );

  // Try calling handleSearch without a callback.
  Assert.throws(
    () => ExtensionSearchHandler.handleSearch(searchData),
    /The keyword provided is not registered/
  );

  // Try getting the description for a keyword which isn't registered.
  Assert.throws(
    () => ExtensionSearchHandler.getDescription(unregisteredKeyword),
    /The keyword provided is not registered/
  );

  // Try setting the default suggestion for a keyword which isn't registered.
  Assert.throws(
    () =>
      ExtensionSearchHandler.setDefaultSuggestion(
        unregisteredKeyword,
        "suggestion"
      ),
    /The keyword provided is not registered/
  );

  // Try calling handleInputCancelled when there is no active input session.
  Assert.throws(
    () => ExtensionSearchHandler.handleInputCancelled(),
    /There is no active input session/
  );

  // Try calling handleInputEntered when there is no active input session.
  Assert.throws(
    () =>
      ExtensionSearchHandler.handleInputEntered(
        anotherKeyword,
        `${anotherKeyword} test`,
        "tab"
      ),
    /There is no active input session/
  );

  // Start a session by calling handleSearch with the registered keyword.
  searchData = {
    keyword,
    text: `${keyword} test`,
  };
  ExtensionSearchHandler.handleSearch(searchData, () => {});

  // Try providing suggestions for an unregistered keyword.
  Assert.throws(
    () => ExtensionSearchHandler.addSuggestions(unregisteredKeyword, 0, []),
    /The keyword provided is not registered/
  );

  // Try providing suggestions for an inactive keyword.
  Assert.throws(
    () => ExtensionSearchHandler.addSuggestions(anotherKeyword, 0, []),
    /The keyword provided is not apart of an active input session/
  );

  // Try calling handleSearch for an inactive keyword.
  searchData = {
    keyword: anotherKeyword,
    text: `${anotherKeyword} `,
  };
  Assert.throws(
    () => ExtensionSearchHandler.handleSearch(searchData, () => {}),
    /A different input session is already ongoing/
  );

  // Try calling addSuggestions with an old callback ID.
  Assert.throws(
    () => ExtensionSearchHandler.addSuggestions(keyword, 0, []),
    /The callback is no longer active for the keyword provided/
  );

  // Add suggestions with a valid callback ID.
  ExtensionSearchHandler.addSuggestions(keyword, 1, []);

  // Add suggestions again with a valid callback ID.
  ExtensionSearchHandler.addSuggestions(keyword, 1, []);

  // Try calling addSuggestions with a future callback ID.
  Assert.throws(
    () => ExtensionSearchHandler.addSuggestions(keyword, 2, []),
    /The callback is no longer active for the keyword provided/
  );

  // End the input session by calling handleInputCancelled.
  ExtensionSearchHandler.handleInputCancelled();

  // Try calling handleInputCancelled after the session has ended.
  Assert.throws(
    () => ExtensionSearchHandler.handleInputCancelled(),
    /There is no active input sessio/
  );

  // Try calling handleSearch that doesn't have a space after the keyword.
  searchData = {
    keyword: anotherKeyword,
    text: `${anotherKeyword}`,
  };
  Assert.throws(
    () => ExtensionSearchHandler.handleSearch(searchData, () => {}),
    /The text provided must start with/
  );

  // Try calling handleSearch with text starting with the wrong keyword.
  searchData = {
    keyword: anotherKeyword,
    text: `${keyword} test`,
  };
  Assert.throws(
    () => ExtensionSearchHandler.handleSearch(searchData, () => {}),
    /The text provided must start with/
  );

  // Start a new session by calling handleSearch with a different keyword
  searchData = {
    keyword: anotherKeyword,
    text: `${anotherKeyword} test`,
  };
  ExtensionSearchHandler.handleSearch(searchData, () => {});

  // Try adding suggestions again with the same callback ID now that the input session has ended.
  Assert.throws(
    () => ExtensionSearchHandler.addSuggestions(keyword, 1, []),
    /The keyword provided is not apart of an active input session/
  );

  // Add suggestions with a valid callback ID.
  ExtensionSearchHandler.addSuggestions(anotherKeyword, 2, []);

  // Try adding suggestions with a valid callback ID but a different keyword.
  Assert.throws(
    () => ExtensionSearchHandler.addSuggestions(keyword, 2, []),
    /The keyword provided is not apart of an active input session/
  );

  // Try adding suggestions with a valid callback ID but an unregistered keyword.
  Assert.throws(
    () => ExtensionSearchHandler.addSuggestions(unregisteredKeyword, 2, []),
    /The keyword provided is not registered/
  );

  // Set the default suggestion.
  ExtensionSearchHandler.setDefaultSuggestion(anotherKeyword, {
    description: "test result",
  });

  // Try ending the session using handleInputEntered with a different keyword.
  Assert.throws(
    () =>
      ExtensionSearchHandler.handleInputEntered(
        keyword,
        `${keyword} test`,
        "tab"
      ),
    /A different input session is already ongoing/
  );

  // Try calling handleInputEntered with invalid text.
  Assert.throws(
    () =>
      ExtensionSearchHandler.handleInputEntered(anotherKeyword, ` test`, "tab"),
    /The text provided must start with/
  );

  // Try calling handleInputEntered with an invalid disposition.
  Assert.throws(
    () =>
      ExtensionSearchHandler.handleInputEntered(
        anotherKeyword,
        `${anotherKeyword} test`,
        "invalid"
      ),
    /Invalid "where" argument/
  );

  // End the session by calling handleInputEntered.
  ExtensionSearchHandler.handleInputEntered(
    anotherKeyword,
    `${anotherKeyword} test`,
    "tab"
  );

  // Try calling handleInputEntered after the session has ended.
  Assert.throws(
    () =>
      ExtensionSearchHandler.handleInputEntered(
        anotherKeyword,
        `${anotherKeyword} test`,
        "tab"
      ),
    /There is no active input session/
  );

  // Unregister the keyword.
  ExtensionSearchHandler.unregisterKeyword(keyword);

  // Try setting the default suggestion for the unregistered keyword.
  Assert.throws(
    () =>
      ExtensionSearchHandler.setDefaultSuggestion(keyword, {
        description: "test",
      }),
    /The keyword provided is not registered/
  );

  // Try handling a search with the unregistered keyword.
  searchData = {
    keyword,
    text: `${keyword} test`,
  };
  Assert.throws(
    () => ExtensionSearchHandler.handleSearch(searchData, () => {}),
    /The keyword provided is not registered/
  );

  // Try unregistering the keyword again.
  Assert.throws(
    () => ExtensionSearchHandler.unregisterKeyword(keyword),
    /The keyword provided is not registered/
  );

  // Unregister the other keyword.
  ExtensionSearchHandler.unregisterKeyword(anotherKeyword);

  // Try unregistering the word which was never registered.
  Assert.throws(
    () => ExtensionSearchHandler.unregisterKeyword(unregisteredKeyword),
    /The keyword provided is not registered/
  );

  // Try setting the default suggestion for a word that was never registered.
  Assert.throws(
    () =>
      ExtensionSearchHandler.setDefaultSuggestion(unregisteredKeyword, {
        description: "test",
      }),
    /The keyword provided is not registered/
  );

  await cleanup();
});

add_task(async function test_extension_private_browsing() {
  let events = [];
  let mockExtension = {
    emit: message => events.push(message),
    privateBrowsingAllowed: false,
  };

  let keyword = "foo";

  ExtensionSearchHandler.registerKeyword(keyword, mockExtension);

  let searchData = {
    keyword,
    text: `${keyword} test`,
    inPrivateWindow: true,
  };
  let result = await ExtensionSearchHandler.handleSearch(searchData);
  Assert.equal(result, false, "unable to handle search for private window");

  // Try calling handleInputEntered after the session has ended.
  Assert.throws(
    () =>
      ExtensionSearchHandler.handleInputEntered(
        keyword,
        `${keyword} test`,
        "tab"
      ),
    /There is no active input session/
  );

  ExtensionSearchHandler.unregisterKeyword(keyword);
  await cleanup();
});

add_task(async function test_correct_events_are_emitted() {
  let events = [];
  function checkEvents(expectedEvents) {
    Assert.equal(
      events.length,
      expectedEvents.length,
      "The correct number of events fired"
    );
    expectedEvents.forEach((e, i) =>
      Assert.equal(e, events[i], `Expected "${e}" event to fire`)
    );
    events = [];
  }

  let mockExtension = { emit: message => events.push(message) };

  let keyword = "foo";
  let anotherKeyword = "bar";

  ExtensionSearchHandler.registerKeyword(keyword, mockExtension);
  ExtensionSearchHandler.registerKeyword(anotherKeyword, mockExtension);

  let searchData = {
    keyword,
    text: `${keyword} `,
  };
  ExtensionSearchHandler.handleSearch(searchData, () => {});
  checkEvents([ExtensionSearchHandler.MSG_INPUT_STARTED]);

  searchData.text = `${keyword} f`;
  ExtensionSearchHandler.handleSearch(searchData, () => {});
  checkEvents([ExtensionSearchHandler.MSG_INPUT_CHANGED]);

  ExtensionSearchHandler.handleInputEntered(keyword, searchData.text, "tab");
  checkEvents([ExtensionSearchHandler.MSG_INPUT_ENTERED]);

  ExtensionSearchHandler.handleSearch(searchData, () => {});
  checkEvents([
    ExtensionSearchHandler.MSG_INPUT_STARTED,
    ExtensionSearchHandler.MSG_INPUT_CHANGED,
  ]);

  ExtensionSearchHandler.handleInputCancelled();
  checkEvents([ExtensionSearchHandler.MSG_INPUT_CANCELLED]);

  ExtensionSearchHandler.handleSearch(
    {
      keyword: anotherKeyword,
      text: `${anotherKeyword} baz`,
    },
    () => {}
  );
  checkEvents([
    ExtensionSearchHandler.MSG_INPUT_STARTED,
    ExtensionSearchHandler.MSG_INPUT_CHANGED,
  ]);

  ExtensionSearchHandler.handleInputEntered(
    anotherKeyword,
    `${anotherKeyword} baz`,
    "tab"
  );
  checkEvents([ExtensionSearchHandler.MSG_INPUT_ENTERED]);

  ExtensionSearchHandler.unregisterKeyword(keyword);
});

add_task(async function test_removes_suggestion_if_its_content_is_typed_in() {
  let keyword = "test";
  let extensionName = "Foo Bar";

  let mockExtension = {
    name: extensionName,
    emit(message, text, id) {
      if (message === ExtensionSearchHandler.MSG_INPUT_CHANGED) {
        ExtensionSearchHandler.addSuggestions(keyword, id, [
          { content: "foo", description: "first suggestion" },
          { content: "bar", description: "second suggestion" },
          { content: "baz", description: "third suggestion" },
        ]);
        // The API doesn't have a way to notify when addition is complete.
        do_timeout(1000, () => {
          controller.stopSearch();
        });
      }
    },
  };

  ExtensionSearchHandler.registerKeyword(keyword, mockExtension);

  let query = `${keyword} unmatched`;
  let context = createContext(query, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeOmniboxResult(context, {
        heuristic: true,
        keyword,
        description: extensionName,
        content: `${keyword} unmatched`,
      }),
      makeOmniboxResult(context, {
        keyword,
        content: `${keyword} foo`,
        description: "first suggestion",
      }),
      makeOmniboxResult(context, {
        keyword,
        content: `${keyword} bar`,
        description: "second suggestion",
      }),
      makeOmniboxResult(context, {
        keyword,
        content: `${keyword} baz`,
        description: "third suggestion",
      }),
    ],
  });

  query = `${keyword} foo`;
  context = createContext(query, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeOmniboxResult(context, {
        heuristic: true,
        keyword,
        description: extensionName,
        content: `${keyword} foo`,
      }),
      makeOmniboxResult(context, {
        keyword,
        content: `${keyword} bar`,
        description: "second suggestion",
      }),
      makeOmniboxResult(context, {
        keyword,
        content: `${keyword} baz`,
        description: "third suggestion",
      }),
    ],
  });

  query = `${keyword} bar`;
  context = createContext(query, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeOmniboxResult(context, {
        heuristic: true,
        keyword,
        description: extensionName,
        content: `${keyword} bar`,
      }),
      makeOmniboxResult(context, {
        keyword,
        content: `${keyword} foo`,
        description: "first suggestion",
      }),
      makeOmniboxResult(context, {
        keyword,
        content: `${keyword} baz`,
        description: "third suggestion",
      }),
    ],
  });

  query = `${keyword} baz`;
  context = createContext(query, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeOmniboxResult(context, {
        heuristic: true,
        keyword,
        description: extensionName,
        content: `${keyword} baz`,
      }),
      makeOmniboxResult(context, {
        keyword,
        content: `${keyword} foo`,
        description: "first suggestion",
      }),
      makeOmniboxResult(context, {
        keyword,
        content: `${keyword} bar`,
        description: "second suggestion",
      }),
    ],
  });

  ExtensionSearchHandler.unregisterKeyword(keyword);
  await cleanup();
});

add_task(async function test_extension_results_should_come_first() {
  let keyword = "test";
  let extensionName = "Omnibox Example";

  let uri = Services.io.newURI(`http://a.com/b`);
  await PlacesTestUtils.addVisits([{ uri, title: `${keyword} -` }]);

  let mockExtension = {
    name: extensionName,
    emit(message, text, id) {
      if (message === ExtensionSearchHandler.MSG_INPUT_CHANGED) {
        ExtensionSearchHandler.addSuggestions(keyword, id, [
          { content: "foo", description: "first suggestion" },
          { content: "bar", description: "second suggestion" },
          { content: "baz", description: "third suggestion" },
        ]);
      }
    },
  };

  ExtensionSearchHandler.registerKeyword(keyword, mockExtension);

  // Start an input session before testing MSG_INPUT_CHANGED.
  ExtensionSearchHandler.handleSearch(
    { keyword, text: `${keyword} ` },
    () => {}
  );

  let query = `${keyword} -`;
  let context = createContext(query, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeOmniboxResult(context, {
        heuristic: true,
        keyword,
        description: extensionName,
        content: `${keyword} -`,
      }),
      makeOmniboxResult(context, {
        keyword,
        content: `${keyword} foo`,
        description: "first suggestion",
      }),
      makeOmniboxResult(context, {
        keyword,
        content: `${keyword} bar`,
        description: "second suggestion",
      }),
      makeOmniboxResult(context, {
        keyword,
        content: `${keyword} baz`,
        description: "third suggestion",
      }),
      makeVisitResult(context, {
        uri: `http://a.com/b`,
        title: `${keyword} -`,
      }),
    ],
  });

  ExtensionSearchHandler.unregisterKeyword(keyword);
  await cleanup();
});

add_task(async function test_setting_the_default_suggestion() {
  let keyword = "test";
  let extensionName = "Omnibox Example";

  let mockExtension = {
    name: extensionName,
    emit(message, text, id) {
      if (message === ExtensionSearchHandler.MSG_INPUT_CHANGED) {
        ExtensionSearchHandler.addSuggestions(keyword, id, []);
        // The API doesn't have a way to notify when addition is complete.
        do_timeout(1000, () => {
          controller.stopSearch();
        });
      }
    },
  };

  ExtensionSearchHandler.registerKeyword(keyword, mockExtension);

  ExtensionSearchHandler.setDefaultSuggestion(keyword, {
    description: "hello world",
  });

  let query = `${keyword} search query`;
  let context = createContext(query, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeOmniboxResult(context, {
        heuristic: true,
        keyword,
        description: "hello world",
        content: query,
      }),
    ],
  });

  ExtensionSearchHandler.setDefaultSuggestion(keyword, {
    description: "foo bar",
  });

  context = createContext(query, { isPrivate: false });
  await check_results({
    context,
    searchParam: "enable-actions",
    matches: [
      makeOmniboxResult(context, {
        heuristic: true,
        keyword,
        description: "foo bar",
        content: query,
      }),
    ],
  });

  ExtensionSearchHandler.unregisterKeyword(keyword);
  await cleanup();
});

add_task(async function test_maximum_number_of_suggestions_is_enforced() {
  let keyword = "test";
  let extensionName = "Omnibox Example";

  let mockExtension = {
    name: extensionName,
    emit(message, text, id) {
      if (message === ExtensionSearchHandler.MSG_INPUT_CHANGED) {
        ExtensionSearchHandler.addSuggestions(keyword, id, [
          { content: "a", description: "first suggestion" },
          { content: "b", description: "second suggestion" },
          { content: "c", description: "third suggestion" },
          { content: "d", description: "fourth suggestion" },
          { content: "e", description: "fifth suggestion" },
          { content: "f", description: "sixth suggestion" },
          { content: "g", description: "seventh suggestion" },
          { content: "h", description: "eigth suggestion" },
          { content: "i", description: "ninth suggestion" },
          { content: "j", description: "tenth suggestion" },
        ]);
        // The API doesn't have a way to notify when addition is complete.
        do_timeout(1000, () => {
          controller.stopSearch();
        });
      }
    },
  };

  ExtensionSearchHandler.registerKeyword(keyword, mockExtension);

  // Start an input session before testing MSG_INPUT_CHANGED.
  ExtensionSearchHandler.handleSearch(
    { keyword, text: `${keyword} ` },
    () => {}
  );

  let query = `${keyword} #`;
  let context = createContext(query, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeOmniboxResult(context, {
        heuristic: true,
        keyword,
        description: extensionName,
        content: `${keyword} #`,
      }),
      makeOmniboxResult(context, {
        keyword,
        content: `${keyword} a`,
        description: "first suggestion",
      }),
      makeOmniboxResult(context, {
        keyword,
        content: `${keyword} b`,
        description: "second suggestion",
      }),
      makeOmniboxResult(context, {
        keyword,
        content: `${keyword} c`,
        description: "third suggestion",
      }),
      makeOmniboxResult(context, {
        keyword,
        content: `${keyword} d`,
        description: "fourth suggestion",
      }),
      makeOmniboxResult(context, {
        keyword,
        content: `${keyword} e`,
        description: "fifth suggestion",
      }),
    ],
  });

  ExtensionSearchHandler.unregisterKeyword(keyword);
  await cleanup();
});

add_task(async function conflicting_alias() {
  Services.prefs.setBoolPref(SUGGEST_PREF, true);
  Services.prefs.setBoolPref(SUGGEST_ENABLED_PREF, true);

  let engine = await addTestSuggestionsEngine();
  let keyword = "test";
  engine.alias = keyword;
  let oldDefaultEngine = await Services.search.getDefault();
  Services.search.setDefault(engine);

  let extensionName = "Omnibox Example";

  let mockExtension = {
    name: extensionName,
    emit(message, text, id) {
      if (message === ExtensionSearchHandler.MSG_INPUT_CHANGED) {
        ExtensionSearchHandler.addSuggestions(keyword, id, [
          { content: "foo", description: "first suggestion" },
          { content: "bar", description: "second suggestion" },
          { content: "baz", description: "third suggestion" },
        ]);
        // The API doesn't have a way to notify when addition is complete.
        do_timeout(1000, () => {
          controller.stopSearch();
        });
      }
    },
  };

  ExtensionSearchHandler.registerKeyword(keyword, mockExtension);
  let query = `${keyword} unmatched`;
  let context = createContext(query, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeOmniboxResult(context, {
        heuristic: true,
        keyword,
        description: extensionName,
        content: `${keyword} unmatched`,
      }),
      makeOmniboxResult(context, {
        keyword,
        content: `${keyword} foo`,
        description: "first suggestion",
      }),
      makeOmniboxResult(context, {
        keyword,
        content: `${keyword} bar`,
        description: "second suggestion",
      }),
      makeOmniboxResult(context, {
        keyword,
        content: `${keyword} baz`,
        description: "third suggestion",
      }),
      makeSearchResult(context, {
        query: "unmatched",
        engineName: SUGGESTIONS_ENGINE_NAME,
        alias: keyword,
        suggestion: "unmatched",
      }),
      makeSearchResult(context, {
        query: "unmatched",
        engineName: SUGGESTIONS_ENGINE_NAME,
        alias: keyword,
        suggestion: "unmatched foo",
      }),
      makeSearchResult(context, {
        query: "unmatched",
        engineName: SUGGESTIONS_ENGINE_NAME,
        alias: keyword,
        suggestion: "unmatched bar",
      }),
    ],
  });

  Services.search.setDefault(oldDefaultEngine);
  Services.prefs.setBoolPref(SUGGEST_PREF, false);
  Services.prefs.setBoolPref(SUGGEST_ENABLED_PREF, false);
  await cleanup();
});
