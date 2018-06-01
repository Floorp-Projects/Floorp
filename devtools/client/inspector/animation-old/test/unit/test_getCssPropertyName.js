/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {require} = ChromeUtils.import("resource://devtools/shared/Loader.jsm", {});
const {getCssPropertyName} = require("devtools/client/inspector/animation-old/utils");

const TEST_DATA = [{
  jsName: "alllowercase",
  cssName: "alllowercase"
}, {
  jsName: "borderWidth",
  cssName: "border-width"
}, {
  jsName: "borderTopRightRadius",
  cssName: "border-top-right-radius"
}];

function run_test() {
  for (const {jsName, cssName} of TEST_DATA) {
    equal(getCssPropertyName(jsName), cssName);
  }
}
