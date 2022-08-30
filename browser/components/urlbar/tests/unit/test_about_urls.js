/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { AboutPagesUtils } = ChromeUtils.import(
  "resource://gre/modules/AboutPagesUtils.jsm"
);

testEngine_setup();

// "about:ab" should match "about:about"
add_task(async function aboutAb() {
  let context = createContext("about:ab", { isPrivate: false });
  await check_results({
    context,
    autofilled: "about:about",
    completed: "about:about",
    matches: [
      makeVisitResult(context, {
        uri: "about:about",
        title: "about:about",
        heuristic: true,
      }),
    ],
  });
});

// "about:Ab" should match "about:about"
add_task(async function aboutAb() {
  let context = createContext("about:Ab", { isPrivate: false });
  await check_results({
    context,
    autofilled: "about:About",
    completed: "about:about",
    matches: [
      makeVisitResult(context, {
        uri: "about:about",
        title: "about:about",
        heuristic: true,
      }),
    ],
  });
});

// "about:about" should match "about:about"
add_task(async function aboutAbout() {
  let context = createContext("about:about", { isPrivate: false });
  await check_results({
    context,
    autofilled: "about:about",
    completed: "about:about",
    matches: [
      makeVisitResult(context, {
        uri: "about:about",
        title: "about:about",
        heuristic: true,
      }),
    ],
  });
});

// "about:a" should complete to "about:about" and also match "about:addons"
add_task(async function aboutAboutAndAboutAddons() {
  let context = createContext("about:a", { isPrivate: false });
  await check_results({
    context,
    search: "about:a",
    autofilled: "about:about",
    completed: "about:about",
    matches: [
      makeVisitResult(context, {
        uri: "about:about",
        title: "about:about",
        heuristic: true,
      }),
      makeVisitResult(context, {
        uri: "about:addons",
        title: "about:addons",
        tags: null,
        providerName: "AboutPages",
      }),
    ],
  });
});

// "about:" by itself matches a list of about: pages and nothing else
add_task(async function aboutColonMatchesOnlyAboutPages() {
  // We generate 9 about: page results because there are 10 results total,
  // and the first result is the heuristic result.
  function getFirst9AboutPages() {
    const aboutPageNames = AboutPagesUtils.visibleAboutUrls.slice(0, 9);
    const aboutPageResults = aboutPageNames.map(aboutPageName => {
      return makeVisitResult(context, {
        uri: aboutPageName,
        title: aboutPageName,
        tags: null,
        providerName: "AboutPages",
      });
    });
    return aboutPageResults;
  }

  let context = createContext("about:", { isPrivate: false });
  await check_results({
    context,
    search: "about:",
    matches: [
      makeSearchResult(context, {
        engineName: SUGGESTIONS_ENGINE_NAME,
        providerName: "HeuristicFallback",
        heuristic: true,
      }),
      ...getFirst9AboutPages(),
    ],
  });
});

// Results for about: pages do not match webpage titles from the user's history
add_task(async function aboutResultsDoNotMatchTitlesInHistory() {
  await PlacesTestUtils.addVisits([
    {
      uri: Services.io.newURI("http://example.com/guide/config/"),
      title: "Guide to config in Firefox",
    },
  ]);

  let context = createContext("about:config", { isPrivate: false });
  await check_results({
    context,
    search: "about:config",
    matches: [
      makeVisitResult(context, {
        uri: "about:config",
        title: "about:config",
        heuristic: true,
        providerName: "Autofill",
      }),
    ],
  });
});

// Tests that about: pages are shown after general results.
add_task(async function after_general() {
  await PlacesTestUtils.addVisits([
    {
      uri: Services.io.newURI("http://example.com/guide/aboutaddons/"),
      title: "Guide to about:addons in Firefox",
    },
  ]);

  let context = createContext("about:a", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        uri: "about:about",
        title: "about:about",
        heuristic: true,
        providerName: "Autofill",
      }),
      makeVisitResult(context, {
        uri: "http://example.com/guide/aboutaddons/",
        title: "Guide to about:addons in Firefox",
      }),
      makeVisitResult(context, {
        uri: "about:addons",
        title: "about:addons",
        tags: null,
        providerName: "AboutPages",
      }),
    ],
  });
  await cleanupPlaces();
});
