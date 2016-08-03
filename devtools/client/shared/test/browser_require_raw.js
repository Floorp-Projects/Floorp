/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { BrowserLoader } = Cu.import("resource://devtools/client/shared/browser-loader.js", {});

const { require: browserRequire } = BrowserLoader({
  baseURI: "resource://devtools/client/shared/",
  window: this
});

const variableFileContents = browserRequire("raw!devtools/client/themes/variables.css");

function test() {
  ok(variableFileContents.length > 0, "raw browserRequire worked");
  finish();
}
