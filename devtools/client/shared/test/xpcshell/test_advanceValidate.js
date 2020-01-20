/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests the advanceValidate function from rule-view.js.

const { require } = ChromeUtils.import("resource://devtools/shared/Loader.jsm");
const { advanceValidate } = require("devtools/client/inspector/shared/utils");
const { KeyCodes } = require("devtools/client/shared/keycodes");

//                            1         2         3
//                  0123456789012345678901234567890
const sampleInput = '\\symbol "string" url(somewhere)';

function testInsertion(where, result, testName) {
  info(testName);
  equal(
    advanceValidate(KeyCodes.DOM_VK_SEMICOLON, sampleInput, where),
    result,
    "testing advanceValidate at " + where
  );
}

function run_test() {
  testInsertion(4, true, "inside a symbol");
  testInsertion(1, false, "after a backslash");
  testInsertion(8, true, "after whitespace");
  testInsertion(11, false, "inside a string");
  testInsertion(24, false, "inside a URL");
  testInsertion(31, true, "at the end");
}
