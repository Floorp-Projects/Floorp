/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* eslint no-unused-vars: [2, {"vars": "local"}] */

const { require } = ChromeUtils.importESModule(
  "resource://devtools/shared/loader/Loader.sys.mjs"
);

const Store = require("resource://devtools/client/responsive/store.js");

const DevToolsUtils = require("resource://devtools/shared/DevToolsUtils.js");

Services.prefs.setBoolPref("devtools.testing", true);
registerCleanupFunction(() => {
  Services.prefs.clearUserPref("devtools.testing");
});
