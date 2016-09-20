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
  "console.clear()",
  "console.count('bar')",
  "console.assert(false, {message: 'foobar'})"
];

let consoleApi = new Map(consoleApiCommands.map(
  cmd => [cmd, {keys: [cmd], code: cmd}]));

consoleApi.set("console.trace()", {
  keys: ["console.trace()"],
  code: `
function bar() {
  console.trace()
}
function foo() {
  bar()
}

foo()
`});

consoleApi.set("console.time('bar')", {
  keys: ["console.time('bar')", "console.timeEnd('bar')"],
  code: `
console.time("bar");
console.timeEnd("bar");
`});

// Evaluation Result

const evaluationResultCommands = [
  "new Date(0)"
];

let evaluationResult = new Map(evaluationResultCommands.map(cmd => [cmd, cmd]));

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
  evaluationResult,
  pageError,
};
