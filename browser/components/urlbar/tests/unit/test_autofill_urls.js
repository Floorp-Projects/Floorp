/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const HEURISTIC_FALLBACK_PROVIDERNAME = "HeuristicFallback";
const UNIFIEDCOMPLETE_PROVIDERNAME = "UnifiedComplete";

// "example.com/foo/" should match http://example.com/foo/.
testEngine_setup();

add_task(async function multipleSlashes() {
  await PlacesTestUtils.addVisits([
    {
      uri: "http://example.com/foo/",
    },
  ]);
  let context = createContext("example.com/foo/", { isPrivate: false });
  await check_results({
    context,
    autofilled: "example.com/foo/",
    completed: "http://example.com/foo/",
    matches: [
      makeVisitResult(context, {
        uri: "http://example.com/foo/",
        title: "example.com/foo/",
        heuristic: true,
      }),
    ],
  });
  await cleanupPlaces();
});

// "example.com:8888/f" should match http://example.com:8888/foo.
add_task(async function port() {
  await PlacesTestUtils.addVisits([
    {
      uri: "http://example.com:8888/foo",
    },
  ]);
  let context = createContext("example.com:8888/f", { isPrivate: false });
  await check_results({
    context,
    autofilled: "example.com:8888/foo",
    completed: "http://example.com:8888/foo",
    matches: [
      makeVisitResult(context, {
        uri: "http://example.com:8888/foo",
        title: "example.com:8888/foo",
        heuristic: true,
      }),
    ],
  });
  await cleanupPlaces();
});

// "example.com:8999/f" should *not* autofill http://example.com:8888/foo.
add_task(async function portNoMatch() {
  await PlacesTestUtils.addVisits([
    {
      uri: "http://example.com:8888/foo",
    },
  ]);
  let context = createContext("example.com:8999/f", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        uri: "http://example.com:8999/f",
        title: "http://example.com:8999/f",
        iconUri: "page-icon:http://example.com:8999/",
        heuristic: true,
        providerName: HEURISTIC_FALLBACK_PROVIDERNAME,
      }),
    ],
  });
  await cleanupPlaces();
});

// autofill to the next slash
add_task(async function port() {
  await PlacesTestUtils.addVisits([
    {
      uri: "http://example.com:8888/foo/bar/baz",
    },
  ]);
  let context = createContext("example.com:8888/foo/b", { isPrivate: false });
  await check_results({
    context,
    autofilled: "example.com:8888/foo/bar/",
    completed: "http://example.com:8888/foo/bar/",
    matches: [
      makeVisitResult(context, {
        uri: "http://example.com:8888/foo/bar/",
        title: "example.com:8888/foo/bar/",
        heuristic: true,
      }),
      makeVisitResult(context, {
        uri: "http://example.com:8888/foo/bar/baz",
        title: "test visit for http://example.com:8888/foo/bar/baz",
        tags: [],
        providerName: UNIFIEDCOMPLETE_PROVIDERNAME,
      }),
    ],
  });
  await cleanupPlaces();
});

// autofill to the next slash, end of url
add_task(async function port() {
  await PlacesTestUtils.addVisits([
    {
      uri: "http://example.com:8888/foo/bar/baz",
    },
  ]);
  let context = createContext("example.com:8888/foo/bar/b", {
    isPrivate: false,
  });
  await check_results({
    context,
    autofilled: "example.com:8888/foo/bar/baz",
    completed: "http://example.com:8888/foo/bar/baz",
    matches: [
      makeVisitResult(context, {
        uri: "http://example.com:8888/foo/bar/baz",
        title: "example.com:8888/foo/bar/baz",
        heuristic: true,
      }),
    ],
  });
  await cleanupPlaces();
});
