/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;
var { require } = Cu.import("resource://devtools/shared/Loader.jsm", {});

var Services = require("Services");
var DevToolsUtils = require("devtools/shared/DevToolsUtils");
DevToolsUtils.testing = true;
DevToolsUtils.dumpn.wantLogging = true;
DevToolsUtils.dumpv.wantVerbose = false;

const { storeFactory } = require("devtools/client/webconsole/new-console-output/store");

const testPackets = new Map();
testPackets.set("console.log", {
  "from": "server1.conn4.child1/consoleActor2",
  "type": "consoleAPICall",
  "message": {
    "arguments": [
      "foobar",
      "test"
    ],
    "columnNumber": 1,
    "counter": null,
    "filename": "file:///test.html",
    "functionName": "",
    "groupName": "",
    "level": "log",
    "lineNumber": 1,
    "private": false,
    "styles": [],
    "timeStamp": 1455064271115,
    "timer": null,
    "workerType": "none",
    "category": "webdev"
  }
});
