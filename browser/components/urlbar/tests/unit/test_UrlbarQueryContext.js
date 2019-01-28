/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(function test_constructor() {
  Assert.throws(() => new UrlbarQueryContext(),
    /Missing or empty enableAutofill provided to UrlbarQueryContext/,
    "Should throw with no arguments");

  Assert.throws(() => new UrlbarQueryContext({
    enableAutofill: true,
    isPrivate: false,
    maxResults: 1,
    searchString: "foo",
  }), /Missing or empty lastKey provided to UrlbarQueryContext/,
    "Should throw with a missing lastKey parameter");

  Assert.throws(() => new UrlbarQueryContext({
    enableAutofill: true,
    isPrivate: false,
    lastKey: "b",
    searchString: "foo",
  }), /Missing or empty maxResults provided to UrlbarQueryContext/,
    "Should throw with a missing maxResults parameter");

  Assert.throws(() => new UrlbarQueryContext({
    enableAutofill: true,
    lastKey: "b",
    maxResults: 1,
    searchString: "foo",
  }), /Missing or empty isPrivate provided to UrlbarQueryContext/,
    "Should throw with a missing isPrivate parameter");

  Assert.throws(() => new UrlbarQueryContext({
    isPrivate: false,
    lastKey: "b",
    maxResults: 1,
    searchString: "foo",
  }), /Missing or empty enableAutofill provided to UrlbarQueryContext/,
    "Should throw with a missing enableAutofill parameter");

  let qc = new UrlbarQueryContext({
    enableAutofill: false,
    isPrivate: true,
    lastKey: "b",
    maxResults: 1,
    searchString: "foo",
  });

  Assert.strictEqual(qc.enableAutofill, false,
    "Should have saved the correct value for enableAutofill");
  Assert.strictEqual(qc.isPrivate, true,
    "Should have saved the correct value for isPrivate");
  Assert.equal(qc.lastKey, "b",
    "Should have saved the correct value for lastKey");
  Assert.equal(qc.maxResults, 1,
    "Should have saved the correct value for maxResults");
  Assert.equal(qc.searchString, "foo",
    "Should have saved the correct value for searchString");
});
