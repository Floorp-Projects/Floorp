/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

XPCOMUtils.defineLazyModuleGetters(this, {
  KeywordTree: "resource:///modules/UrlbarQuickSuggest.jsm",
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
];

function basicChecks(tree) {
  Assert.equal(tree.get("nomatch").result, null);
  Assert.equal(tree.get("he").result, null);
  Assert.equal(tree.get("hel").result, "helzo foo");
  Assert.equal(tree.get("hel").fullKeyword, "helzo");
  Assert.equal(tree.get("helzo").result, "helzo foo");
  Assert.equal(tree.get("helzo").fullKeyword, "helzo");
  Assert.equal(tree.get("helzo ").result, "helzo foo");
  Assert.equal(tree.get("helzo foo").result, "helzo foo");
  Assert.equal(tree.get("helzo b").result, "helzo bar");
  Assert.equal(tree.get("helzo b").fullKeyword, "helzo bar");
  Assert.equal(tree.get("helzo bar").result, "helzo bar");
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
      he: {
        lzo: {
          "^": "helzo foo",
          " ": { foo: { "^": "helzo foo" }, bar: { "^": "helzo bar" } },
        },
      },
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
