/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const ENGINE_NAME = "engine-suggestions.xml";

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
        iconUri: "",
        providerName: "UnifiedComplete",
      }),
    ],
  });
});

// "about:" should *not* match anything
add_task(async function aboutColonHasNoMatch() {
  let context = createContext("about:", { isPrivate: false });
  await check_results({
    context,
    search: "about:",
    matches: [
      makeSearchResult(context, {
        engineName: ENGINE_NAME,
        providerName: "HeuristicFallback",
        heuristic: true,
      }),
    ],
  });
});
