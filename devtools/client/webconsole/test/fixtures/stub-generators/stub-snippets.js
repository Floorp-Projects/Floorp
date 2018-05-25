/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

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
  "console.log('myarray', ['red', 'green', 'blue'])",
  "console.log('myregex', /a.b.c/)",
  "console.table(['red', 'green', 'blue']);",
  "console.log('myobject', {red: 'redValue', green: 'greenValue', blue: 'blueValue'});",
  "console.debug('debug message');",
  "console.info('info message');",
  "console.error('error message');",
];

let consoleApi = new Map(consoleApiCommands.map(
  cmd => [cmd, {keys: [cmd], code: cmd}]));

consoleApi.set("console.log('mymap')", {
  keys: ["console.log('mymap')"],
  code: `
var map = new Map();
map.set("key1", "value1");
map.set("key2", "value2");
console.log('mymap', map);
`});

consoleApi.set("console.log('myset')", {
  keys: ["console.log('myset')"],
  code: `
console.log('myset', new Set(["a", "b"]));
`});

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
  keys: [
    "console.time('bar')",
    "timerAlreadyExists",
    "console.timeLog('bar') - 1",
    "console.timeLog('bar') - 2",
    "console.timeEnd('bar')",
    "timeEnd.timerDoesntExist",
    "timeLog.timerDoesntExist",
  ],
  code: `
console.time("bar");
console.time("bar");
console.timeLog("bar");
console.timeLog("bar", "second call", {state: 1});
console.timeEnd("bar");
console.timeEnd("bar");
console.timeLog("bar");
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
console.groupEnd();
`});

consoleApi.set("console.groupCollapsed('foo')", {
  keys: ["console.groupCollapsed('foo')", "console.groupEnd('foo')"],
  code: `
console.groupCollapsed("foo");
console.groupEnd();
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
  "color:blue; font-size:1.3em; background:url('http://example.com/test'); position:absolute; top:10px; ",
  "color:red; line-height: 1.5; background:\\165rl('http://example.com/test')"
);
`});

consoleApi.set('console.log("%cHello%c|%cWorld")', {
  keys: ['console.log("%cHello%c|%cWorld")'],
  code: `
  console.log(
    "%cHello%c|%cWorld",
    "color:red",
    "",
    "color: blue"
  );
`});

consoleApi.set("console.group(%cfoo%cbar)", {
  keys: ["console.group(%cfoo%cbar)", "console.groupEnd(%cfoo%cbar)"],
  code: `
console.group(
  "%cfoo%cbar",
  "color:blue;font-size:1.3em;background:url('http://example.com/test');position:absolute;top:10px",
  "color:red;background:\\165rl('http://example.com/test')");
console.groupEnd();
`});

consoleApi.set("console.groupCollapsed(%cfoo%cbaz)", {
  keys: ["console.groupCollapsed(%cfoo%cbaz)", "console.groupEnd(%cfoo%cbaz)"],
  code: `
console.groupCollapsed(
  "%cfoo%cbaz",
  "color:blue;font-size:1.3em;background:url('http://example.com/test');position:absolute;top:10px",
  "color:red;background:\\165rl('http://example.com/test')");
console.groupEnd();
`});

consoleApi.set("console.dir({C, M, Y, K})", {
  keys: ["console.dir({C, M, Y, K})"],
  code: "console.dir({cyan: 'C', magenta: 'M', yellow: 'Y', black: 'K'});"
});

consoleApi.set("console.count", {
  keys: [
    "console.count | default: 1",
    "console.count | default: 2",
    "console.count | test counter: 1",
    "console.count | test counter: 2",
    "console.count | default: 3",
    "console.count | clear",
    "console.count | default: 4",
    "console.count | test counter: 3",
    "console.countReset | test counter: 0",
    "console.countReset | counterDoesntExist",
  ],
  code: `
    console.count();
    console.count();
    console.count("test counter");
    console.count("test counter");
    console.count();
    console.clear();
    console.count();
    console.count("test counter");
    console.countReset("test counter");
    console.countReset("test counter");
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
  "1 + @",
  "inspect({a: 1})",
  "cd(document)",
  "undefined"
];

let evaluationResult = new Map(evaluationResultCommands.map(cmd => [cmd, cmd]));
evaluationResult.set("longString message Error",
  `throw new Error("Long error ".repeat(10000))`);

evaluationResult.set(`eval throw ""`, `throw ""`);
evaluationResult.set(`eval throw "tomato"`, `throw "tomato"`);

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

pageError.set("ReferenceError: asdf is not defined", `
  function bar() {
    asdf()
  }
  function foo() {
    bar()
  }

  foo()
`);

pageError.set("SyntaxError: redeclaration of let a", `
  let a, a;
`);

pageError.set("TypeError longString message",
  `throw new Error("Long error ".repeat(10000))`);

pageError.set(`throw ""`, `throw ""`);
pageError.set(`throw "tomato"`, `throw "tomato"`);

module.exports = {
  consoleApi,
  cssMessage,
  evaluationResult,
  networkEvent,
  pageError,
};
