/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.defineModuleGetter(this, "QueryContext",
  "resource:///modules/UrlbarController.jsm");

add_task(function test_constructor() {
  Assert.throws(() => new QueryContext(),
    /Missing or empty searchString provided to QueryContext/,
    "Should throw with no arguments");

  Assert.throws(() => new QueryContext({
    searchString: "foo",
    maxResults: 1,
    isPrivate: false,
  }), /Missing or empty lastKey provided to QueryContext/,
    "Should throw with a missing lastKey parameter");

  Assert.throws(() => new QueryContext({
    searchString: "foo",
    lastKey: "b",
    isPrivate: false,
  }), /Missing or empty maxResults provided to QueryContext/,
    "Should throw with a missing maxResults parameter");

  Assert.throws(() => new QueryContext({
    searchString: "foo",
    lastKey: "b",
    maxResults: 1,
  }), /Missing or empty isPrivate provided to QueryContext/,
    "Should throw with a missing isPrivate parameter");

  let qc = new QueryContext({
    searchString: "foo",
    lastKey: "b",
    maxResults: 1,
    isPrivate: true,
    autoFill: false,
  });

  Assert.equal(qc.searchString, "foo",
    "Should have saved the correct value for searchString");
  Assert.equal(qc.lastKey, "b",
    "Should have saved the correct value for lastKey");
  Assert.equal(qc.maxResults, 1,
    "Should have saved the correct value for maxResults");
  Assert.strictEqual(qc.isPrivate, true,
    "Should have saved the correct value for isPrivate");
  Assert.strictEqual(qc.autoFill, false,
    "Should have saved the correct value for autoFill");
});
