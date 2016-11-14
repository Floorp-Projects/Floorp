/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var {DebuggerServer} = require("devtools/server/main");
var longString = (new Array(DebuggerServer.LONG_STRING_LENGTH + 4)).join("a");
var initialString = longString.substring(0, DebuggerServer.LONG_STRING_INITIAL_LENGTH);

// Console API

const consoleApiCommands = [
  "console.log('foobar', 'test')",
  "console.log(undefined)",
  "console.warn('danger, will robinson!')",
  "console.log(NaN)",
  "console.log(null)",
  "console.log('\u9f2c')",
  "console.clear()",
  "console.count('bar')",
  "console.assert(false, {message: 'foobar'})",
  "console.log('hello \\nfrom \\rthe \\\"string world!')",
  "console.log('\xFA\u1E47\u0129\xE7\xF6d\xEA \u021B\u0115\u0219\u0165')",
  "console.dirxml(window)",
];

let consoleApi = new Map(consoleApiCommands.map(
  cmd => [cmd, {keys: [cmd], code: cmd}]));

consoleApi.set("console.trace()", {
  keys: ["console.trace()"],
  code: `
function testStacktraceFiltering() {
  console.trace()
}
function foo() {
  testStacktraceFiltering()
}

foo()
`});

consoleApi.set("console.time('bar')", {
  keys: ["console.time('bar')", "console.timeEnd('bar')"],
  code: `
console.time("bar");
console.timeEnd("bar");
`});

consoleApi.set("console.table('bar')", {
  keys: ["console.table('bar')"],
  code: `
console.table('bar');
`});

consoleApi.set("console.table(['a', 'b', 'c'])", {
  keys: ["console.table(['a', 'b', 'c'])"],
  code: `
console.table(['a', 'b', 'c']);
`});

consoleApi.set("console.group('bar')", {
  keys: ["console.group('bar')", "console.groupEnd('bar')"],
  code: `
console.group("bar");
console.groupEnd("bar");
`});

consoleApi.set("console.groupCollapsed('foo')", {
  keys: ["console.groupCollapsed('foo')", "console.groupEnd('foo')"],
  code: `
console.groupCollapsed("foo");
console.groupEnd("foo");
`});

consoleApi.set("console.group()", {
  keys: ["console.group()", "console.groupEnd()"],
  code: `
console.group();
console.groupEnd();
`});

consoleApi.set("console.log(%cfoobar)", {
  keys: ["console.log(%cfoobar)"],
  code: `
console.log(
  "%cfoo%cbar",
  "color:blue;font-size:1.3em;background:url('http://example.com/test');position:absolute;top:10px",
  "color:red;background:\\165rl('http://example.com/test')");
`});

// CSS messages
const cssMessage = new Map();

cssMessage.set("Unknown property", `
p {
  such-unknown-property: wow;
}
`);

cssMessage.set("Invalid property value", `
p {
  padding-top: invalid value;
}
`);

// Evaluation Result
const evaluationResultCommands = [
  "new Date(0)",
  "asdf()",
  "1 + @"
];

let evaluationResult = new Map(evaluationResultCommands.map(cmd => [cmd, cmd]));

// Network Event

let networkEvent = new Map();

networkEvent.set("GET request", {
  keys: ["GET request"],
  code: `
let i = document.createElement("img");
i.src = "inexistent.html";
`});

networkEvent.set("XHR GET request", {
  keys: ["XHR GET request"],
  code: `
const xhr = new XMLHttpRequest();
xhr.open("GET", "inexistent.html");
xhr.send();
`});

networkEvent.set("XHR POST request", {
  keys: ["XHR POST request"],
  code: `
const xhr = new XMLHttpRequest();
xhr.open("POST", "inexistent.html");
xhr.send();
`});

// Page Error

let pageError = new Map();

pageError.set("Reference Error", `
  function bar() {
    asdf()
  }
  function foo() {
    bar()
  }

  foo()
`);

module.exports = {
  consoleApi,
  cssMessage,
  evaluationResult,
  networkEvent,
  pageError,
};
