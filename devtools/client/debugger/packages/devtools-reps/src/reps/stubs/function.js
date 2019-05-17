/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const stubs = new Map();
stubs.set("Named", {
  type: "object",
  class: "Function",
  actor: "server1.conn6.obj35",
  extensible: true,
  frozen: false,
  sealed: false,
  isAsync: false,
  isGenerator: false,
  name: "testName",
  displayName: "testName",
  location: {
    url: "debugger eval code",
    line: 1,
  },
});

stubs.set("UserNamed", {
  type: "object",
  class: "Function",
  actor: "server1.conn6.obj35",
  extensible: true,
  frozen: false,
  sealed: false,
  isAsync: false,
  isGenerator: false,
  name: "testName",
  userDisplayName: "testUserName",
  displayName: "testName",
  location: {
    url: "debugger eval code",
    line: 1,
  },
});

stubs.set("VarNamed", {
  type: "object",
  class: "Function",
  actor: "server1.conn7.obj41",
  extensible: true,
  frozen: false,
  sealed: false,
  isAsync: false,
  isGenerator: false,
  displayName: "testVarName",
  location: {
    url: "debugger eval code",
    line: 1,
  },
});

stubs.set("Anon", {
  type: "object",
  class: "Function",
  actor: "server1.conn7.obj45",
  extensible: true,
  frozen: false,
  sealed: false,
  isAsync: false,
  isGenerator: false,
  location: {
    url: "debugger eval code",
    line: 1,
  },
});

stubs.set("LongName", {
  type: "object",
  class: "Function",
  actor: "server1.conn7.obj67",
  extensible: true,
  frozen: false,
  sealed: false,
  isAsync: false,
  isGenerator: false,
  name:
    "looooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo" +
    "ooooooooooooooooooooooooooooooooooong",
  displayName:
    "loooooooooooooooooooooooooooooooooooooooooooooooooooooooooo" +
    "oooooooooooooooooooooooooooooooooooooooooong",
  location: {
    url: "debugger eval code",
    line: 1,
  },
});

stubs.set("AsyncFunction", {
  type: "object",
  class: "Function",
  actor: "server1.conn7.obj45",
  extensible: true,
  frozen: false,
  sealed: false,
  isAsync: true,
  isGenerator: false,
  name: "waitUntil2017",
  displayName: "waitUntil2017",
  location: {
    url: "debugger eval code",
    line: 1,
  },
});

stubs.set("AnonAsyncFunction", {
  type: "object",
  class: "Function",
  actor: "server1.conn7.obj45",
  extensible: true,
  frozen: false,
  sealed: false,
  isAsync: true,
  isGenerator: false,
  location: {
    url: "debugger eval code",
    line: 1,
  },
});

stubs.set("GeneratorFunction", {
  type: "object",
  class: "Function",
  actor: "server1.conn7.obj45",
  extensible: true,
  frozen: false,
  sealed: false,
  isAsync: false,
  isGenerator: true,
  name: "fib",
  displayName: "fib",
  location: {
    url: "debugger eval code",
    line: 1,
  },
});

stubs.set("AnonGeneratorFunction", {
  type: "object",
  class: "Function",
  actor: "server1.conn7.obj45",
  extensible: true,
  frozen: false,
  sealed: false,
  isAsync: false,
  isGenerator: true,
  location: {
    url: "debugger eval code",
    line: 1,
  },
});

stubs.set("getRandom", {
  type: "object",
  actor: "server1.conn7.child1/obj984",
  class: "Function",
  extensible: true,
  frozen: false,
  sealed: false,
  ownPropertyLength: 3,
  name: "getRandom",
  displayName: "getRandom",
  location: {
    url: "https://nchevobbe.github.io/demo/console-test-app.html",
    line: 314,
  },
});

stubs.set("EvaledInDebuggerFunction", {
  type: "object",
  actor: "server1.conn2.child1/obj29",
  class: "Function",
  extensible: true,
  frozen: false,
  sealed: false,
  ownPropertyLength: 3,
  name: "evaledInDebugger",
  displayName: "evaledInDebugger",
  location: {
    url: "debugger eval code",
    line: 1,
  },
});

stubs.set("ObjectProperty", {
  type: "object",
  class: "Function",
  actor: "server1.conn7.obj45",
  extensible: true,
  frozen: false,
  sealed: false,
  isAync: false,
  isGenerator: false,
  name: "$",
  displayName: "jQuery",
  location: {
    url: "debugger eval code",
    line: 1,
  },
});

module.exports = stubs;
