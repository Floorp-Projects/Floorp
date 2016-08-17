/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var Cu = Components.utils;
const { require } = Cu.import("resource://devtools/shared/Loader.jsm", {});
const { parseJSONString, isJSON } = require("devtools/client/webconsole/net/utils/json");

// Test data
const simpleJson = '{"name":"John"}';
const jsonInFunc = 'someFunc({"name":"John"})';

const json1 = "{'a': 1}";
const json2 = "  {'a': 1}";
const json3 = "\t {'a': 1}";
const json4 = "\n\n\t {'a': 1}";
const json5 = "\n\n\t ";

const textMimeType = "text/plain";
const jsonMimeType = "text/javascript";
const unknownMimeType = "text/unknown";

/**
 * Testing API provided by webconsole/net/utils/json.js
 */
function run_test() {
  // parseJSONString
  equal(parseJSONString(simpleJson).name, "John");
  equal(parseJSONString(jsonInFunc).name, "John");

  // isJSON
  equal(isJSON(textMimeType, json1), true);
  equal(isJSON(textMimeType, json2), true);
  equal(isJSON(jsonMimeType, json3), true);
  equal(isJSON(jsonMimeType, json4), true);

  equal(isJSON(unknownMimeType, json1), true);
  equal(isJSON(textMimeType, json1), true);

  equal(isJSON(unknownMimeType), false);
  equal(isJSON(unknownMimeType, json5), false);
}
