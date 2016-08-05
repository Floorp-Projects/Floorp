/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* exported storeFactory */

"use strict";

var { utils: Cu } = Components;
var { require } = Cu.import("resource://devtools/shared/Loader.jsm", {});
const Services = require("Services");

var DevToolsUtils = require("devtools/shared/DevToolsUtils");
var flags = require("devtools/shared/flags");
flags.testing = true;
flags.wantLogging = true;
flags.wantVerbose = false;

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

testPackets.set("console.clear", {
  "from": "server1.conn1.child1/consoleActor2",
  "type": "consoleAPICall",
  "message": {
    "arguments": [],
    "columnNumber": 1,
    "counter": null,
    "filename": "debugger eval code",
    "functionName": "",
    "groupName": "",
    "level": "clear",
    "lineNumber": 1,
    "private": false,
    "timeStamp": 1462571355142,
    "timer": null,
    "workerType": "none",
    "styles": [],
    "category": "webdev"
  }
});
