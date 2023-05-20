/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests utility function in `classnames.js`
 */

const classnames = require("resource://devtools/client/shared/classnames.js");

add_task(async function () {
  Assert.equal(
    classnames(),
    "",
    "Returns an empty string when called with no params"
  );
  Assert.equal(
    classnames(null, undefined, false),
    "",
    "Returns an empty string when called with only falsy params"
  );
  Assert.equal(
    classnames("hello"),
    "hello",
    "Returns expected result when string is passed"
  );
  Assert.equal(
    classnames("hello", "", "world"),
    "hello world",
    "Doesn't add extra spaces for empty strings"
  );
  Assert.equal(
    classnames("hello", null, undefined, false, "world"),
    "hello world",
    "Doesn't add extra spaces for falsy values"
  );
  Assert.equal(
    classnames("hello", { nice: true, blue: 42, world: {} }),
    "hello nice blue world",
    "Add property key when property value is truthy"
  );
  Assert.equal(
    classnames("hello", { nice: false, blue: null, world: false }),
    "hello",
    "Does not add property key when property value is falsy"
  );
  Assert.equal(
    classnames("hello", { nice: true }, { blue: true }, "world"),
    "hello nice blue world",
    "Handles multiple objects"
  );
});
