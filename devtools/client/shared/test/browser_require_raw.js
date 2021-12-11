/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { BrowserLoader } = ChromeUtils.import(
  "resource://devtools/client/shared/browser-loader.js"
);

const { require: browserRequire } = BrowserLoader({
  baseURI: "resource://devtools/client/shared/",
  window,
});

const variableFileContents = browserRequire(
  "raw!chrome://devtools/skin/variables.css"
);

function test() {
  ok(variableFileContents.length > 0, "raw browserRequire worked");
  delete window.getBrowserLoaderForWindow;
  finish();
}
