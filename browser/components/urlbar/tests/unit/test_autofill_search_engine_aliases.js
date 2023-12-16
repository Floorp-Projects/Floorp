/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests autofilling search engine token ("@") aliases.

"use strict";

const TEST_ENGINE_NAME = "test autofill aliases";
const TEST_ENGINE_ALIAS = "@autofilltest";

add_setup(async () => {
  // Add an engine with an "@" alias.
  await SearchTestUtils.installSearchExtension({
    name: TEST_ENGINE_NAME,
    keyword: TEST_ENGINE_ALIAS,
  });
});

// Searching for @autofi should autofill to @autofilltest.
add_task(async function basic() {
  // Add a history visit that should normally match but for the fact that the
  // search uses an @ alias.  When an @ alias is autofilled, there should be no
  // other matches except the autofill heuristic match.
  await PlacesTestUtils.addVisits({
    uri: "http://example.com/",
    title: TEST_ENGINE_ALIAS,
  });

  let search = TEST_ENGINE_ALIAS.substr(
    0,
    Math.round(TEST_ENGINE_ALIAS.length / 2)
  );
  let autofilledValue = TEST_ENGINE_ALIAS + " ";
  let context = createContext(search, { isPrivate: false });
  await check_results({
    context,
    autofilled: autofilledValue,
    matches: [
      makeSearchResult(context, {
        engineName: TEST_ENGINE_NAME,
        alias: TEST_ENGINE_ALIAS,
        query: "",
        providesSearchMode: true,
        heuristic: false,
      }),
    ],
  });
  await cleanupPlaces();
});

// Searching for @AUTOFI should autofill to @AUTOFIlltest, preserving the case
// in the search string.
add_task(async function preserveCase() {
  // Add a history visit that should normally match but for the fact that the
  // search uses an @ alias.  When an @ alias is autofilled, there should be no
  // other matches except the autofill heuristic match.
  await PlacesTestUtils.addVisits({
    uri: "http://example.com/",
    title: TEST_ENGINE_ALIAS,
  });

  let search = TEST_ENGINE_ALIAS.toUpperCase().substr(
    0,
    Math.round(TEST_ENGINE_ALIAS.length / 2)
  );
  let alias = search + TEST_ENGINE_ALIAS.substr(search.length);

  let autofilledValue = alias + " ";
  let context = createContext(search, { isPrivate: false });
  await check_results({
    context,
    autofilled: autofilledValue,
    matches: [
      makeSearchResult(context, {
        engineName: TEST_ENGINE_NAME,
        alias,
        query: "",
        providesSearchMode: true,
        heuristic: false,
      }),
    ],
  });
  await cleanupPlaces();
});
