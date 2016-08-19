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
];

let consoleApi = new Map(consoleApiCommands.map(cmd => [cmd, cmd]));

consoleApi.set("console.trace()",
`
function bar() {
  console.trace()
}
function foo() {
  bar()
}

foo()
`);

consoleApi.set("console.time()",
`
console.time()
console.timeEnd()
`);

// Evaluation Result

const evaluationResultCommands = [
  "new Date(0)"
];

let evaluationResult = new Map(evaluationResultCommands.map(cmd => [cmd, cmd]));

// Page Error

const pageErrorCommands = [
  "asdf()",
];

let pageError = new Map(pageErrorCommands.map(cmd => [cmd, cmd]));

module.exports = {
  consoleApi,
  evaluationResult,
  pageError,
};
