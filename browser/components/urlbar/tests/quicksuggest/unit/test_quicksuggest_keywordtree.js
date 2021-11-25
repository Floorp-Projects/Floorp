/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

XPCOMUtils.defineLazyModuleGetters(this, {
  KeywordTree: "resource:///modules/UrlbarQuickSuggest.jsm",
  UrlbarQuickSuggest: "resource:///modules/UrlbarQuickSuggest.jsm",
});

let data = [
  {
    term: "helzo foo",
    keywords: ["hel", "helz", "helzo", "helzo f", "helzo fo", "helzo foo"],
  },
  {
    term: "helzo bar",
    keywords: ["helzo b", "helzo ba", "helzo bar"],
  },

  // test10 and test11 must be in the following order to test bug 1697678.
  { term: "test10", keywords: ["kids sn", "kids sneakers"] },
  { term: "test11", keywords: ["ki", "kin", "kind", "kindl", "kindle"] },

  // test12 and test13 must be in the following order to test bug 1698534.
  // Searching for "websi" should match test12 because search strings must match
  // keywords exactly, no prefix matching.
  { term: "test12", keywords: ["websi"] },
  { term: "test13", keywords: ["webs", "websit"] },
];

let dataByTerm = data.reduce((map, record) => {
  map[record.term] = record;
  return map;
}, {});

function basicChecks(tree) {
  let tests = [
    {
      query: "nomatch",
      term: null,
    },

    {
      query: "he",
      term: null,
    },
    {
      query: "hel",
      term: "helzo foo",
      fullKeyword: "helzo",
    },
    {
      query: "helzo",
      term: "helzo foo",
      fullKeyword: "helzo",
    },
    {
      query: "helzo ",
      term: null,
    },
    {
      query: "helzo f",
      term: "helzo foo",
      fullKeyword: "helzo foo",
    },
    {
      query: "helzo foo",
      term: "helzo foo",
      fullKeyword: "helzo foo",
    },
    {
      query: "helzo b",
      term: "helzo bar",
      fullKeyword: "helzo bar",
    },
    {
      query: "helzo bar",
      term: "helzo bar",
      fullKeyword: "helzo bar",
    },

    {
      query: "f",
      term: null,
    },
    {
      query: "fo",
      term: null,
    },
    {
      query: "foo",
      term: null,
    },

    {
      query: "ki",
      term: "test11",
      fullKeyword: "kindle",
    },
    {
      query: "kin",
      term: "test11",
      fullKeyword: "kindle",
    },
    {
      query: "kind",
      term: "test11",
      fullKeyword: "kindle",
    },
    {
      query: "kid",
      term: null,
    },
    {
      query: "kids",
      term: null,
    },
    {
      query: "kids ",
      term: null,
    },
    {
      query: "kids s",
      term: null,
    },
    {
      query: "kids sn",
      term: "test10",
      fullKeyword: "kids sneakers",
    },

    {
      query: "web",
      term: null,
    },
    {
      query: "webs",
      term: "test13",
      fullKeyword: "websit",
    },
    {
      query: "websi",
      term: "test12",
      fullKeyword: "websi",
    },
    {
      query: "websit",
      term: "test13",
      fullKeyword: "websit",
    },
    {
      query: "website",
      term: null,
    },
  ];

  for (let test of tests) {
    info("Checking " + JSON.stringify(test));
    let { query, term, fullKeyword } = test;
    Assert.equal(tree.get(query), term);
    if (term) {
      Assert.equal(
        UrlbarQuickSuggest.getFullKeyword(query, dataByTerm[term].keywords),
        fullKeyword
      );
    }
  }
}

function createTree() {
  let tree = new KeywordTree();

  for (let { term, keywords } of data) {
    keywords.forEach(keyword => tree.set(keyword, term));
  }
  return tree;
}

add_task(async function test_basic() {
  basicChecks(createTree());
});

add_task(async function test_deserialisation() {
  let str = JSON.stringify(createTree().toJSONObject());
  let newTree = new KeywordTree();
  newTree.fromJSON(JSON.parse(str));
  basicChecks(newTree);
});

add_task(async function test_flatten() {
  let tree = createTree();
  tree.flatten();

  Assert.deepEqual(
    {
      hel: {
        "^": "helzo foo",
        z: {
          "^": "helzo foo",
          o: {
            "^": "helzo foo",
            " ": {
              f: {
                "^": "helzo foo",
                o: { "^": "helzo foo", "o^": "helzo foo" },
              },
              b: {
                "^": "helzo bar",
                a: { "^": "helzo bar", "r^": "helzo bar" },
              },
            },
          },
        },
      },
      ki: {
        "^": "test11",
        n: {
          "^": "test11",
          d: { "^": "test11", l: { "^": "test11", "e^": "test11" } },
        },
        "ds sn": { "^": "test10", "eakers^": "test10" },
      },
      webs: { i: { "^": "test12", "t^": "test13" }, "^": "test13" },
    },
    tree.toJSONObject()
  );
  basicChecks(tree);
});

add_task(async function test_reserved() {
  let tree = createTree();
  try {
    tree.set("helzo^foo");
    Assert.ok(false, "Should thrown when using reserved characters");
  } catch (e) {
    Assert.ok(true, "Did throw when using reserved characters");
  }
});
