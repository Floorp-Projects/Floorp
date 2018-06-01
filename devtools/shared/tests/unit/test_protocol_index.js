/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";
const { lazyLoadFront } = require("devtools/shared/specs/index");
const Types = require("devtools/shared/specs/index").__TypesForTests;
const { getType } = require("devtools/shared/protocol").types;

function run_test() {
  test_index_is_alphabetically_sorted();
  test_specs();
  test_fronts();
}

// Check alphabetic order of specs defined in devtools/shared/specs/index.js,
// in order to ease its maintenance and readability.
function test_index_is_alphabetically_sorted() {
  let lastSpec = "";
  for (const type of Types) {
    const spec = type.spec;
    if (lastSpec && spec < lastSpec) {
      ok(false, `Spec definition for "${spec}" should be before "${lastSpec}"`);
    }
    lastSpec = spec;
  }
  ok(true, "Specs index is alphabetically sorted");
}

function test_specs() {
  for (const type of Types) {
    for (const typeName of type.types) {
      ok(getType(typeName), `${typeName} spec is defined`);
    }
  }
  ok(true, "Specs are all accessible");
}

function test_fronts() {
  for (const item of Types) {
    if (!item.front) {
      continue;
    }
    for (const typeName of item.types) {
      lazyLoadFront(typeName);
      const type = getType(typeName);
      ok(type, `Front for ${typeName} has a spec`);
      ok(type.frontClass, `${typeName} has a front correctly defined`);
    }
  }
  ok(true, "Front are all accessible");
}
