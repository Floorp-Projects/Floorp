/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* eslint no-unused-vars: [2, {"vars": "local"}] */

const { require } = ChromeUtils.import("resource://devtools/shared/Loader.jsm");

const Services = require("Services");
const Store = require("devtools/client/responsive/store");

const DevToolsUtils = require("devtools/shared/DevToolsUtils");

Services.prefs.setBoolPref("devtools.testing", true);
registerCleanupFunction(() => {
  Services.prefs.clearUserPref("devtools.testing");
});
