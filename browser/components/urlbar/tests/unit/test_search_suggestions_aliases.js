/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that an engine with suggestions works with our alias autocomplete
 * behavior.
 */

const DEFAULT_ENGINE_NAME = "TestDefaultEngine";
const SUGGEST_PREF = "browser.urlbar.suggest.searches";
const SUGGEST_ENABLED_PREF = "browser.search.suggest.enabled";
const HISTORY_TITLE = "fire";

// We make sure that aliases and search terms are correctly recognized when they
// are separated by each of these different types of spaces and combinations of
// spaces.  U+3000 is the ideographic space in CJK and is commonly used by CJK
// speakers.
const TEST_SPACES = [" ", "\u3000", " \u3000", "\u3000 "];

let engine;
let port;

add_setup(async function () {
  engine = await addTestSuggestionsEngine();
  port = engine.getSubmission("").uri.port;

  // Set a mock engine as the default so we don't hit the network below when we
  // do searches that return the default engine heuristic result.
  await SearchTestUtils.installSearchExtension(
    {
      name: DEFAULT_ENGINE_NAME,
      search_url: "https://my.search.com/",
    },
    { setAsDefault: true }
  );

  // History matches should not appear with @aliases, so this visit should not
  // appear when searching with @aliases below.
  await PlacesTestUtils.addVisits({
    uri: engine.searchForm,
    title: HISTORY_TITLE,
  });
});

// A non-token alias without a trailing space shouldn't be recognized as a
// keyword.  It should be treated as part of the search string.
add_task(async function nonTokenAlias_noTrailingSpace() {
  Services.prefs.setBoolPref(
    "browser.search.separatePrivateDefault.ui.enabled",
    false
  );

  let alias = "moz";
  engine.alias = alias;
  Assert.equal(engine.alias, alias);
  let context = createContext(alias, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: DEFAULT_ENGINE_NAME,
        query: alias,
        heuristic: true,
      }),
    ],
  });
  Services.prefs.clearUserPref(
    "browser.search.separatePrivateDefault.ui.enabled"
  );
});

// A non-token alias with a trailing space should be recognized as a keyword,
// and the history result should be included.
add_task(async function nonTokenAlias_trailingSpace() {
  let alias = "moz";
  engine.alias = alias;
  Assert.equal(engine.alias, alias);

  for (let isPrivate of [false, true]) {
    for (let spaces of TEST_SPACES) {
      info(
        "Testing: " + JSON.stringify({ isPrivate, spaces: codePoints(spaces) })
      );
      let context = createContext(alias + spaces, { isPrivate });
      await check_results({
        context,
        matches: [
          makeSearchResult(context, {
            engineName: SUGGESTIONS_ENGINE_NAME,
            alias,
            query: "",
            heuristic: true,
          }),
          makeVisitResult(context, {
            uri: `http://localhost:${port}/search?q=`,
            title: HISTORY_TITLE,
          }),
        ],
      });
    }
  }
});

// Search for "alias HISTORY_TITLE" with a non-token alias in a non-private
// context.  The remote suggestions and history result should be shown.
add_task(async function nonTokenAlias_history_nonPrivate() {
  let alias = "moz";
  engine.alias = alias;
  Assert.equal(engine.alias, alias);
  for (let spaces of TEST_SPACES) {
    info("Testing: " + JSON.stringify({ spaces: codePoints(spaces) }));
    let context = createContext(alias + spaces + HISTORY_TITLE, {
      isPrivate: false,
    });
    await check_results({
      context,
      matches: [
        makeSearchResult(context, {
          engineName: SUGGESTIONS_ENGINE_NAME,
          alias,
          query: HISTORY_TITLE,
          heuristic: true,
        }),
        makeSearchResult(context, {
          engineName: SUGGESTIONS_ENGINE_NAME,
          alias,
          query: HISTORY_TITLE,
          suggestion: `${HISTORY_TITLE} foo`,
        }),
        makeSearchResult(context, {
          engineName: SUGGESTIONS_ENGINE_NAME,
          alias,
          query: HISTORY_TITLE,
          suggestion: `${HISTORY_TITLE} bar`,
        }),
        makeVisitResult(context, {
          uri: `http://localhost:${port}/search?q=`,
          title: HISTORY_TITLE,
        }),
      ],
    });
  }
});

// Search for "alias HISTORY_TITLE" with a non-token alias in a private context.
// The history result should be shown, but not the remote suggestions.
add_task(async function nonTokenAlias_history_private() {
  let alias = "moz";
  engine.alias = alias;
  Assert.equal(engine.alias, alias);
  for (let spaces of TEST_SPACES) {
    info("Testing: " + JSON.stringify({ spaces: codePoints(spaces) }));
    let context = createContext(alias + spaces + HISTORY_TITLE, {
      isPrivate: true,
    });
    await check_results({
      context,
      matches: [
        makeSearchResult(context, {
          engineName: SUGGESTIONS_ENGINE_NAME,
          alias,
          query: HISTORY_TITLE,
          heuristic: true,
        }),
        makeVisitResult(context, {
          uri: `http://localhost:${port}/search?q=`,
          title: HISTORY_TITLE,
        }),
      ],
    });
  }
});

// A token alias without a trailing space should be autofilled with a trailing
// space and recognized as a keyword with a keyword offer.
add_task(async function tokenAlias_noTrailingSpace() {
  let alias = "@moz";
  engine.alias = alias;
  Assert.equal(engine.alias, alias);
  for (let isPrivate of [false, true]) {
    let context = createContext(alias, { isPrivate });
    await check_results({
      context,
      autofilled: alias + " ",
      matches: [
        makeSearchResult(context, {
          engineName: SUGGESTIONS_ENGINE_NAME,
          alias,
          providesSearchMode: true,
          query: "",
          heuristic: false,
        }),
      ],
    });
  }
});

// A token alias with a trailing space should be recognized as a keyword without
// a keyword offer.
add_task(async function tokenAlias_trailingSpace() {
  let alias = "@moz";
  engine.alias = alias;
  Assert.equal(engine.alias, alias);
  for (let isPrivate of [false, true]) {
    for (let spaces of TEST_SPACES) {
      info(
        "Testing: " + JSON.stringify({ isPrivate, spaces: codePoints(spaces) })
      );
      let context = createContext(alias + spaces, { isPrivate });
      await check_results({
        context,
        matches: [
          makeSearchResult(context, {
            engineName: SUGGESTIONS_ENGINE_NAME,
            alias,
            query: "",
            heuristic: true,
          }),
        ],
      });
    }
  }
});

// Search for "alias HISTORY_TITLE" with a token alias in a non-private context.
// The remote suggestions should be shown, but not the history result.
add_task(async function tokenAlias_history_nonPrivate() {
  let alias = "@moz";
  engine.alias = alias;
  Assert.equal(engine.alias, alias);
  for (let spaces of TEST_SPACES) {
    info("Testing: " + JSON.stringify({ spaces: codePoints(spaces) }));
    let context = createContext(alias + spaces + HISTORY_TITLE, {
      isPrivate: false,
    });
    await check_results({
      context,
      matches: [
        makeSearchResult(context, {
          engineName: SUGGESTIONS_ENGINE_NAME,
          alias,
          query: HISTORY_TITLE,
          heuristic: true,
        }),
        makeSearchResult(context, {
          engineName: SUGGESTIONS_ENGINE_NAME,
          alias,
          query: HISTORY_TITLE,
          suggestion: `${HISTORY_TITLE} foo`,
        }),
        makeSearchResult(context, {
          engineName: SUGGESTIONS_ENGINE_NAME,
          alias,
          query: HISTORY_TITLE,
          suggestion: `${HISTORY_TITLE} bar`,
        }),
      ],
    });
  }
});

// Search for "alias HISTORY_TITLE" with a token alias in a private context.
// Neither the history result nor the remote suggestions should be shown.
add_task(async function tokenAlias_history_private() {
  let alias = "@moz";
  engine.alias = alias;
  Assert.equal(engine.alias, alias);
  for (let spaces of TEST_SPACES) {
    info("Testing: " + JSON.stringify({ spaces: codePoints(spaces) }));
    let context = createContext(alias + spaces + HISTORY_TITLE, {
      isPrivate: true,
    });
    await check_results({
      context,
      matches: [
        makeSearchResult(context, {
          engineName: SUGGESTIONS_ENGINE_NAME,
          alias,
          query: HISTORY_TITLE,
          heuristic: true,
        }),
      ],
    });
  }
});

// Even when they're disabled, suggestions should still be returned when using a
// token alias in a non-private context.
add_task(async function suggestionsDisabled_nonPrivate() {
  Services.prefs.setBoolPref(SUGGEST_PREF, false);
  Services.prefs.setBoolPref(SUGGEST_ENABLED_PREF, true);
  let alias = "@moz";
  engine.alias = alias;
  Assert.equal(engine.alias, alias);
  for (let spaces of TEST_SPACES) {
    info("Testing: " + JSON.stringify({ spaces: codePoints(spaces) }));
    let context = createContext(alias + spaces + "term", { isPrivate: false });
    await check_results({
      context,
      matches: [
        makeSearchResult(context, {
          engineName: SUGGESTIONS_ENGINE_NAME,
          alias,
          query: "term",
          heuristic: true,
        }),
        makeSearchResult(context, {
          engineName: SUGGESTIONS_ENGINE_NAME,
          alias,
          query: "term",
          suggestion: "term foo",
        }),
        makeSearchResult(context, {
          engineName: SUGGESTIONS_ENGINE_NAME,
          alias,
          query: "term",
          suggestion: "term bar",
        }),
      ],
    });
  }
  Services.prefs.clearUserPref(SUGGEST_PREF);
  Services.prefs.clearUserPref(SUGGEST_ENABLED_PREF);
});

// Suggestions should not be returned when using a token alias in a private
// context.
add_task(async function suggestionsDisabled_private() {
  Services.prefs.setBoolPref(SUGGEST_PREF, false);
  Services.prefs.setBoolPref(SUGGEST_ENABLED_PREF, true);
  let alias = "@moz";
  engine.alias = alias;
  Assert.equal(engine.alias, alias);
  for (let spaces of TEST_SPACES) {
    info("Testing: " + JSON.stringify({ spaces: codePoints(spaces) }));
    let context = createContext(alias + spaces + "term", { isPrivate: true });
    await check_results({
      context,
      matches: [
        makeSearchResult(context, {
          engineName: SUGGESTIONS_ENGINE_NAME,
          alias,
          query: "term",
          heuristic: true,
        }),
      ],
    });
    Services.prefs.clearUserPref(SUGGEST_PREF);
    Services.prefs.clearUserPref(SUGGEST_ENABLED_PREF);
  }
});

/**
 * Returns an array of code points in the given string.  Each code point is
 * returned as a hexidecimal string.
 *
 * @param {string} str
 *   The code points of this string will be returned.
 * @returns {Array}
 *   Array of code points in the string, where each is a hexidecimal string.
 */
function codePoints(str) {
  return str.split("").map(s => s.charCodeAt(0).toString(16));
}
