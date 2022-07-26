/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  QueryScorer: "resource:///modules/UrlbarProviderInterventions.sys.mjs",
});

const DISTANCE_THRESHOLD = 1;

const DOCUMENTS = {
  clear: [
    "cache firefox",
    "clear cache firefox",
    "clear cache in firefox",
    "clear cookies firefox",
    "clear firefox cache",
    "clear history firefox",
    "cookies firefox",
    "delete cookies firefox",
    "delete history firefox",
    "firefox cache",
    "firefox clear cache",
    "firefox clear cookies",
    "firefox clear history",
    "firefox cookie",
    "firefox cookies",
    "firefox delete cookies",
    "firefox delete history",
    "firefox history",
    "firefox not loading pages",
    "history firefox",
    "how to clear cache",
    "how to clear history",
  ],
  refresh: [
    "firefox crashing",
    "firefox keeps crashing",
    "firefox not responding",
    "firefox not working",
    "firefox refresh",
    "firefox slow",
    "how to reset firefox",
    "refresh firefox",
    "reset firefox",
  ],
  update: [
    "download firefox",
    "download mozilla",
    "firefox browser",
    "firefox download",
    "firefox for mac",
    "firefox for windows",
    "firefox free download",
    "firefox install",
    "firefox installer",
    "firefox latest version",
    "firefox mac",
    "firefox quantum",
    "firefox update",
    "firefox version",
    "firefox windows",
    "get firefox",
    "how to update firefox",
    "install firefox",
    "mozilla download",
    "mozilla firefox 2019",
    "mozilla firefox 2020",
    "mozilla firefox download",
    "mozilla firefox for mac",
    "mozilla firefox for windows",
    "mozilla firefox free download",
    "mozilla firefox mac",
    "mozilla firefox update",
    "mozilla firefox windows",
    "mozilla update",
    "update firefox",
    "update mozilla",
    "www.firefox.com",
  ],
};

const VARIATIONS = new Map([["firefox", ["fire fox", "fox fire", "foxfire"]]]);

let tests = [
  {
    query: "firefox",
    matches: [
      { id: "clear", score: Infinity },
      { id: "refresh", score: Infinity },
      { id: "update", score: Infinity },
    ],
  },
  {
    query: "bogus",
    matches: [
      { id: "clear", score: Infinity },
      { id: "refresh", score: Infinity },
      { id: "update", score: Infinity },
    ],
  },
  {
    query: "no match",
    matches: [
      { id: "clear", score: Infinity },
      { id: "refresh", score: Infinity },
      { id: "update", score: Infinity },
    ],
  },

  // clear
  {
    query: "firefox histo",
    matches: [
      { id: "clear", score: Infinity },
      { id: "refresh", score: Infinity },
      { id: "update", score: Infinity },
    ],
  },
  {
    query: "firefox histor",
    matches: [
      { id: "clear", score: 1 },
      { id: "refresh", score: Infinity },
      { id: "update", score: Infinity },
    ],
  },
  {
    query: "firefox history",
    matches: [
      { id: "clear", score: 0 },
      { id: "refresh", score: Infinity },
      { id: "update", score: Infinity },
    ],
  },
  {
    query: "firefox history we'll keep matching once we match",
    matches: [
      { id: "clear", score: 0 },
      { id: "refresh", score: Infinity },
      { id: "update", score: Infinity },
    ],
  },

  {
    query: "firef history",
    matches: [
      { id: "clear", score: Infinity },
      { id: "refresh", score: Infinity },
      { id: "update", score: Infinity },
    ],
  },
  {
    query: "firefo history",
    matches: [
      { id: "clear", score: 1 },
      { id: "refresh", score: Infinity },
      { id: "update", score: Infinity },
    ],
  },
  {
    query: "firefo histor",
    matches: [
      { id: "clear", score: 2 },
      { id: "refresh", score: Infinity },
      { id: "update", score: Infinity },
    ],
  },
  {
    query: "firefo histor we'll keep matching once we match",
    matches: [
      { id: "clear", score: 2 },
      { id: "refresh", score: Infinity },
      { id: "update", score: Infinity },
    ],
  },

  {
    query: "fire fox history",
    matches: [
      { id: "clear", score: 0 },
      { id: "refresh", score: Infinity },
      { id: "update", score: Infinity },
    ],
  },
  {
    query: "fox fire history",
    matches: [
      { id: "clear", score: 0 },
      { id: "refresh", score: Infinity },
      { id: "update", score: Infinity },
    ],
  },
  {
    query: "foxfire history",
    matches: [
      { id: "clear", score: 0 },
      { id: "refresh", score: Infinity },
      { id: "update", score: Infinity },
    ],
  },

  // refresh
  {
    query: "firefox sl",
    matches: [
      { id: "clear", score: Infinity },
      { id: "refresh", score: Infinity },
      { id: "update", score: Infinity },
    ],
  },
  {
    query: "firefox slo",
    matches: [
      { id: "refresh", score: 1 },
      { id: "clear", score: Infinity },
      { id: "update", score: Infinity },
    ],
  },
  {
    query: "firefox slow",
    matches: [
      { id: "refresh", score: 0 },
      { id: "clear", score: Infinity },
      { id: "update", score: Infinity },
    ],
  },
  {
    query: "firefox slow we'll keep matching once we match",
    matches: [
      { id: "refresh", score: 0 },
      { id: "clear", score: Infinity },
      { id: "update", score: Infinity },
    ],
  },

  {
    query: "firef slow",
    matches: [
      { id: "clear", score: Infinity },
      { id: "refresh", score: Infinity },
      { id: "update", score: Infinity },
    ],
  },
  {
    query: "firefo slow",
    matches: [
      { id: "refresh", score: 1 },
      { id: "clear", score: Infinity },
      { id: "update", score: Infinity },
    ],
  },
  {
    query: "firefo slo",
    matches: [
      { id: "refresh", score: 2 },
      { id: "clear", score: Infinity },
      { id: "update", score: Infinity },
    ],
  },
  {
    query: "firefo slo we'll keep matching once we match",
    matches: [
      { id: "refresh", score: 2 },
      { id: "clear", score: Infinity },
      { id: "update", score: Infinity },
    ],
  },

  {
    query: "fire fox slow",
    matches: [
      { id: "refresh", score: 0 },
      { id: "clear", score: Infinity },
      { id: "update", score: Infinity },
    ],
  },
  {
    query: "fox fire slow",
    matches: [
      { id: "refresh", score: 0 },
      { id: "clear", score: Infinity },
      { id: "update", score: Infinity },
    ],
  },
  {
    query: "foxfire slow",
    matches: [
      { id: "refresh", score: 0 },
      { id: "clear", score: Infinity },
      { id: "update", score: Infinity },
    ],
  },

  // update
  {
    query: "firefox upda",
    matches: [
      { id: "clear", score: Infinity },
      { id: "refresh", score: Infinity },
      { id: "update", score: Infinity },
    ],
  },
  {
    query: "firefox updat",
    matches: [
      { id: "update", score: 1 },
      { id: "clear", score: Infinity },
      { id: "refresh", score: Infinity },
    ],
  },
  {
    query: "firefox update",
    matches: [
      { id: "update", score: 0 },
      { id: "clear", score: Infinity },
      { id: "refresh", score: Infinity },
    ],
  },
  {
    query: "firefox update we'll keep matching once we match",
    matches: [
      { id: "update", score: 0 },
      { id: "clear", score: Infinity },
      { id: "refresh", score: Infinity },
    ],
  },

  {
    query: "firef update",
    matches: [
      { id: "clear", score: Infinity },
      { id: "refresh", score: Infinity },
      { id: "update", score: Infinity },
    ],
  },
  {
    query: "firefo update",
    matches: [
      { id: "update", score: 1 },
      { id: "clear", score: Infinity },
      { id: "refresh", score: Infinity },
    ],
  },
  {
    query: "firefo updat",
    matches: [
      { id: "update", score: 2 },
      { id: "clear", score: Infinity },
      { id: "refresh", score: Infinity },
    ],
  },
  {
    query: "firefo updat we'll keep matching once we match",
    matches: [
      { id: "update", score: 2 },
      { id: "clear", score: Infinity },
      { id: "refresh", score: Infinity },
    ],
  },

  {
    query: "fire fox update",
    matches: [
      { id: "update", score: 0 },
      { id: "clear", score: Infinity },
      { id: "refresh", score: Infinity },
    ],
  },
  {
    query: "fox fire update",
    matches: [
      { id: "update", score: 0 },
      { id: "clear", score: Infinity },
      { id: "refresh", score: Infinity },
    ],
  },
  {
    query: "foxfire update",
    matches: [
      { id: "update", score: 0 },
      { id: "clear", score: Infinity },
      { id: "refresh", score: Infinity },
    ],
  },
];

add_task(async function test() {
  let qs = new QueryScorer({
    distanceThreshold: DISTANCE_THRESHOLD,
    variations: VARIATIONS,
  });

  for (let [id, phrases] of Object.entries(DOCUMENTS)) {
    qs.addDocument({ id, phrases });
  }

  for (let { query, matches } of tests) {
    let actual = qs
      .score(query)
      .map(result => ({ id: result.document.id, score: result.score }));
    Assert.deepEqual(actual, matches, `Query: "${query}"`);
  }
});
