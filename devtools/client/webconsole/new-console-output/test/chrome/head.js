/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var { utils: Cu } = Components;

var { require } = Cu.import("resource://devtools/shared/Loader.jsm", {});
var { Assert } = require("resource://testing-common/Assert.jsm");
var { BrowserLoader } = Cu.import("resource://devtools/client/shared/browser-loader.js", {});
var { Task } = require("devtools/shared/task");

var { require: browserRequire } = BrowserLoader({
  baseURI: "resource://devtools/client/webconsole/",
  window: this
});
