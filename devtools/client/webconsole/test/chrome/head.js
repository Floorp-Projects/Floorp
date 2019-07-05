/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* exported Task, browserRequire */

"use strict";

var { BrowserLoader } = ChromeUtils.import(
  "resource://devtools/client/shared/browser-loader.js"
);

var { require: browserRequire } = BrowserLoader({
  baseURI: "resource://devtools/client/webconsole/",
  window,
});
