/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// The autoFill.searchEngines pref autofills the domains of engines registered
// with the search service.  That's what this test checks.  It's a different
// path in UnifiedComplete.js from normal moz_places autofill, which is tested
// in test_autofill_origins.js and test_autofill_urls.js.

"use strict";

const ENGINE_NAME = "TestEngine";

add_task(async function searchEngines() {
  Services.prefs.setBoolPref("browser.urlbar.autoFill.searchEngines", true);
  Services.prefs.setBoolPref("browser.urlbar.suggest.searches", false);
  Services.prefs.setBoolPref(
    "browser.search.separatePrivateDefault.ui.enabled",
    false
  );

  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("browser.urlbar.autoFill.searchEngines");
    Services.prefs.clearUserPref("browser.urlbar.suggest.searches");
    Services.prefs.clearUserPref(
      "browser.search.separatePrivateDefault.ui.enabled"
    );
  });

  let schemes = ["http", "https"];
  for (let i = 0; i < schemes.length; i++) {
    let scheme = schemes[i];
    let engine = await Services.search.addEngineWithDetails(ENGINE_NAME, {
      method: "GET",
      template: scheme + "://www.example.com/",
      searchGetParams: "q={searchTerms}",
    });

    let context = createContext("ex", { isPrivate: false });
    await check_results({
      context,
      autofilled: "example.com/",
      matches: [
        makePrioritySearchResult(context, {
          engineName: ENGINE_NAME,
          heuristic: true,
        }),
      ],
    });

    context = createContext("example.com", { isPrivate: false });
    await check_results({
      context,
      autofilled: "example.com/",
      matches: [
        makePrioritySearchResult(context, {
          engineName: ENGINE_NAME,
          heuristic: true,
        }),
      ],
    });

    context = createContext("example.com/", { isPrivate: false });
    await check_results({
      context,
      autofilled: "example.com/",
      matches: [
        makePrioritySearchResult(context, {
          engineName: ENGINE_NAME,
          heuristic: true,
        }),
      ],
    });

    context = createContext("www.ex", { isPrivate: false });
    await check_results({
      context,
      autofilled: "www.example.com/",
      matches: [
        makePrioritySearchResult(context, {
          engineName: ENGINE_NAME,
          heuristic: true,
        }),
      ],
    });

    context = createContext("www.example.com", { isPrivate: false });
    await check_results({
      context,
      autofilled: "www.example.com/",
      matches: [
        makePrioritySearchResult(context, {
          engineName: ENGINE_NAME,
          heuristic: true,
        }),
      ],
    });

    context = createContext("www.example.com/", { isPrivate: false });
    await check_results({
      context,
      autofilled: "www.example.com/",
      matches: [
        makePrioritySearchResult(context, {
          engineName: ENGINE_NAME,
          heuristic: true,
        }),
      ],
    });

    context = createContext(scheme + "://ex", { isPrivate: false });
    await check_results({
      context,
      autofilled: scheme + "://example.com/",
      matches: [
        makePrioritySearchResult(context, {
          engineName: ENGINE_NAME,
          heuristic: true,
        }),
      ],
    });

    context = createContext(scheme + "://example.com", { isPrivate: false });
    await check_results({
      context,
      autofilled: scheme + "://example.com/",
      matches: [
        makePrioritySearchResult(context, {
          engineName: ENGINE_NAME,
          heuristic: true,
        }),
      ],
    });

    context = createContext(scheme + "://example.com/", { isPrivate: false });
    await check_results({
      context,
      autofilled: scheme + "://example.com/",
      matches: [
        makePrioritySearchResult(context, {
          engineName: ENGINE_NAME,
          heuristic: true,
        }),
      ],
    });

    context = createContext(scheme + "://www.ex", { isPrivate: false });
    await check_results({
      context,
      autofilled: scheme + "://www.example.com/",
      matches: [
        makePrioritySearchResult(context, {
          engineName: ENGINE_NAME,
          heuristic: true,
        }),
      ],
    });

    context = createContext(scheme + "://www.example.com", {
      isPrivate: false,
    });
    await check_results({
      context,
      autofilled: scheme + "://www.example.com/",
      matches: [
        makePrioritySearchResult(context, {
          engineName: ENGINE_NAME,
          heuristic: true,
        }),
      ],
    });

    context = createContext(scheme + "://www.example.com/", {
      isPrivate: false,
    });
    await check_results({
      context,
      search: scheme + "://www.example.com/",
      autofilled: scheme + "://www.example.com/",
      matches: [
        makePrioritySearchResult(context, {
          engineName: ENGINE_NAME,
          heuristic: true,
        }),
      ],
    });

    // We should just get a normal heuristic result from HeuristicFallback for
    // these queries.
    let otherScheme = schemes[(i + 1) % schemes.length];
    context = createContext(otherScheme + "://ex", { isPrivate: false });
    await check_results({
      context,
      search: otherScheme + "://ex",
      matches: [
        makeVisitResult(context, {
          uri: otherScheme + "://ex/",
          title: otherScheme + "://ex/",
          iconUri: "",
          heuristic: true,
        }),
      ],
    });
    context = createContext(otherScheme + "://www.ex", { isPrivate: false });
    await check_results({
      context,
      search: otherScheme + "://www.ex",
      matches: [
        makeVisitResult(context, {
          uri: otherScheme + "://www.ex/",
          title: otherScheme + "://www.ex/",
          iconUri: "",
          heuristic: true,
        }),
      ],
    });

    context = createContext("example/", { isPrivate: false });
    await check_results({
      context,
      matches: [
        makeVisitResult(context, {
          uri: "http://example/",
          title: "http://example/",
          heuristic: true,
        }),
      ],
    });

    await Services.search.removeEngine(engine);
  }
});
