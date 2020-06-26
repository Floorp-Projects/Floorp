/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that an engine with suggestions works with our alias autocomplete
 * behavior.
 */

const SUGGESTIONS_ENGINE_NAME = "engine-suggestions.xml";
const SUGGEST_PREF = "browser.urlbar.suggest.searches";
const SUGGEST_ENABLED_PREF = "browser.search.suggest.enabled";

let engine;

add_task(async function setup() {
  engine = await addTestSuggestionsEngine();
});

add_task(async function engineWithSuggestions() {
  // History matches should not appear with @ aliases, so this visit/match
  // should not appear when searching with the @ alias below.
  let historyTitle = "fire";
  await PlacesTestUtils.addVisits({
    uri: engine.searchForm,
    title: historyTitle,
  });

  // Search in both a non-private and private context.
  for (let private of [false, true]) {
    // Use a normal alias and then one with an "@".  For the @ alias, the only
    // matches should be the search suggestions -- no history matches.
    for (let alias of ["moz", "@moz"]) {
      engine.alias = alias;
      Assert.equal(engine.alias, alias);

      // Search for "alias"
      let context = createContext(`${alias}`, { isPrivate: private });
      let expectedMatches = [
        makeSearchResult(context, {
          engineName: SUGGESTIONS_ENGINE_NAME,
          alias,
          query: "",
          heuristic: true,
        }),
      ];
      if (alias[0] != "@") {
        expectedMatches.push(
          makeVisitResult(context, {
            uri: "http://localhost:9000/search?terms=",
            title: historyTitle,
          })
        );
      }

      await check_results({
        context,
        matches: expectedMatches,
      });

      // Search for "alias " (trailing space)
      context = createContext(`${alias} `, { isPrivate: private });
      expectedMatches = [
        makeSearchResult(context, {
          engineName: SUGGESTIONS_ENGINE_NAME,
          alias,
          query: "",
          heuristic: true,
        }),
      ];
      if (alias[0] != "@") {
        expectedMatches.push(
          makeVisitResult(context, {
            uri: "http://localhost:9000/search?terms=",
            title: historyTitle,
          })
        );
      }
      await check_results({
        context,
        matches: expectedMatches,
      });

      // Search for "alias historyTitle" -- Include the history title so that
      // the history result is eligible to be shown.  Whether or not it's
      // actually shown depends on the alias: If it's an @ alias, it shouldn't
      // be shown.
      context = createContext(`${alias} ${historyTitle}`, {
        isPrivate: private,
      });
      expectedMatches = [
        makeSearchResult(context, {
          engineName: SUGGESTIONS_ENGINE_NAME,
          alias,
          query: historyTitle,
          heuristic: true,
        }),
      ];
      // Suggestions should be shown in a non-private context but not in a
      // private context.
      if (!private) {
        expectedMatches.push(
          makeSearchResult(context, {
            engineName: SUGGESTIONS_ENGINE_NAME,
            alias,
            query: historyTitle,
            suggestion: `${historyTitle} foo`,
          }),
          makeSearchResult(context, {
            engineName: SUGGESTIONS_ENGINE_NAME,
            alias,
            query: historyTitle,
            suggestion: `${historyTitle} bar`,
          })
        );
      }
      if (alias[0] != "@") {
        expectedMatches.push(
          makeVisitResult(context, {
            uri: "http://localhost:9000/search?terms=",
            title: historyTitle,
          })
        );
      }
      await check_results({
        context,
        matches: expectedMatches,
      });
    }
  }

  engine.alias = "";
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesUtils.history.clear();
});

add_task(async function disabled_urlbarSuggestions() {
  Services.prefs.setBoolPref(SUGGEST_PREF, false);
  Services.prefs.setBoolPref(SUGGEST_ENABLED_PREF, true);
  let alias = "@moz";
  engine.alias = alias;
  Assert.equal(engine.alias, alias);

  for (let private of [false, true]) {
    let context = createContext(`${alias} term`, { isPrivate: private });
    let expectedMatches = [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        alias,
        query: "term",
        heuristic: true,
      }),
    ];

    if (!private) {
      expectedMatches.push(
        makeSearchResult(context, {
          engineName: SUGGESTIONS_ENGINE_NAME,
          alias,
          query: "term",
          suggestion: "term foo",
        })
      );
      expectedMatches.push(
        makeSearchResult(context, {
          engineName: SUGGESTIONS_ENGINE_NAME,
          alias,
          query: "term",
          suggestion: "term bar",
        })
      );
    }
    await check_results({
      context,
      matches: expectedMatches,
    });
  }

  engine.alias = "";
  Services.prefs.clearUserPref(SUGGEST_PREF);
  Services.prefs.clearUserPref(SUGGEST_ENABLED_PREF);
});
