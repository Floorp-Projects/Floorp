/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* eslint no-unused-vars: [2, {"vars": "local", "args": "none"}] */
/* import-globals-from ../../test/head.js */

// Load the NetMonitor head.js file to share its API.
var netMonitorHead = "chrome://mochitests/content/browser/devtools/client/netmonitor/test/head.js";
Services.scriptloader.loadSubScript(netMonitorHead, this);

// Directory with HAR related test files.
const HAR_EXAMPLE_URL = "http://example.com/browser/devtools/client/netmonitor/har/test/";
