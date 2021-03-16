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
    keywords: ["hel", "helz", "helzo", "helzo f", "helzo fo"],
  },
  {
    term: "helzo bar",
    keywords: ["helzo b", "helzo ba"],
  },
  {
    term: "test1",
    keywords: ["aaa", "aaab", "aaa111", "aaab111"],
  },
  {
    term: "test2",
    keywords: ["aaa", "aaab", "aaa222", "aaab222"],
  },

  // These two "test10" and "test11" objects must be in the following order to
  // test bug 1697678.
  { term: "test10", keywords: ["kids sn", "kids sneakers"] },
  { term: "test11", keywords: ["ki", "kin", "kind", "kindl", "kindle"] },

  // test12 and test13 must be in the following order to test bug 1698534.
  // Searching for "websi" should match test12 because search strings must match
  // keywords exactly, no prefix matching.
  { term: "test12", keywords: ["websi"] },
  { term: "test13", keywords: ["webs", "websit"] },
];

function basicChecks(tree) {
  UrlbarQuickSuggest._results = new Map();
  for (let { term, keywords } of data) {
    UrlbarQuickSuggest._results.set(term, { keywords: keywords.concat(term) });
  }

  Assert.equal(tree.get("nomatch").result, null);
  Assert.equal(tree.get("he").result, null);
  Assert.equal(tree.get("hel").result, "helzo foo");
  Assert.equal(tree.get("hel").fullKeyword, "helzo");
  Assert.equal(tree.get("helzo").result, "helzo foo");
  Assert.equal(tree.get("helzo").fullKeyword, "helzo");
  Assert.equal(tree.get("helzo ").result, null);
  Assert.equal(tree.get("helzo foo").result, "helzo foo");
  Assert.equal(tree.get("helzo b").result, "helzo bar");
  Assert.equal(tree.get("helzo b").fullKeyword, "helzo bar");
  Assert.equal(tree.get("helzo bar").result, "helzo bar");

  Assert.equal(tree.get("f").result, null);
  Assert.equal(tree.get("fo").result, null);
  Assert.equal(tree.get("foo").result, null);

  Assert.equal(tree.get("b").result, null);
  Assert.equal(tree.get("ba").result, null);
  Assert.equal(tree.get("bar").result, null);

  Assert.equal(tree.get("aa").result, null);
  Assert.equal(tree.get("aaa").result, "test2");
  Assert.equal(tree.get("aaa").fullKeyword, "aaab222");
  Assert.equal(tree.get("aaab").result, "test2");
  Assert.equal(tree.get("aaab").fullKeyword, "aaab222");
  Assert.equal(tree.get("aaab1").result, null);
  Assert.equal(tree.get("aaab2").result, null);
  Assert.equal(tree.get("aaa1").result, null);
  Assert.equal(tree.get("aaa2").result, null);

  Assert.equal(tree.get("ki").result, "test11");
  Assert.equal(tree.get("ki").fullKeyword, "kindle");
  Assert.equal(tree.get("kin").result, "test11");
  Assert.equal(tree.get("kin").fullKeyword, "kindle");
  Assert.equal(tree.get("kind").result, "test11");
  Assert.equal(tree.get("kind").fullKeyword, "kindle");
  Assert.equal(tree.get("kid").result, null);
  Assert.equal(tree.get("kids").result, null);
  Assert.equal(tree.get("kids ").result, null);
  Assert.equal(tree.get("kids s").result, null);
  Assert.equal(tree.get("kids sn").result, "test10");
  Assert.equal(tree.get("kids sn").fullKeyword, "kids sneakers");

  Assert.equal(tree.get("web").result, null);
  Assert.equal(tree.get("webs").result, "test13");
  Assert.equal(tree.get("webs").fullKeyword, "websit");
  Assert.equal(tree.get("websi").result, "test12");
  Assert.equal(tree.get("websi").fullKeyword, "websi");
  Assert.equal(tree.get("websit").result, "test13");
  Assert.equal(tree.get("websit").fullKeyword, "websit");
  Assert.equal(tree.get("website").result, null);
}

function createTree() {
  let tree = new KeywordTree();

  for (let { term, keywords } of data) {
    keywords.forEach(keyword => tree.set(keyword, term));
    tree.set(term, term);
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
      k: {
        i: {
          "^": "test11",
          "ds s": { n: { "^": "test10", eaker: { s: { "^": "test10" } } } },
          ndle: { "^": "test11" },
        },
      },
      he: {
        lzo: {
          "^": "helzo foo",
          " ": { foo: { "^": "helzo foo" }, bar: { "^": "helzo bar" } },
        },
      },
      aa: {
        a: {
          "11": { "1": { "^": "test1" } },
          "22": { "2": { "^": "test2" } },
          "^": "test2",
          b: {
            "11": { "1": { "^": "test1" } },
            "22": { "2": { "^": "test2" } },
            "^": "test2",
          },
        },
      },
      test: {
        "1": {
          "0": { "^": "test10" },
          "1": { "^": "test11" },
          "2": { "^": "test12" },
          "3": { "^": "test13" },
          "^": "test1",
        },
        "2": { "^": "test2" },
      },
      web: { s: { i: { "^": "test12", t: { "^": "test13" } }, "^": "test13" } },
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
