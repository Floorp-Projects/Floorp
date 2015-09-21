/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

var { Services } = Cu.import("resource://gre/modules/Services.jsm", {});

// Load the NetMonitor head.js file to share its API.
var netMonitorHead = "chrome://mochitests/content/browser/browser/devtools/netmonitor/test/head.js";
Services.scriptloader.loadSubScript(netMonitorHead, this);

// Directory with HAR related test files.
const HAR_EXAMPLE_URL = "http://example.com/browser/browser/devtools/netmonitor/har/test/";
