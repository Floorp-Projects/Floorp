/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Test the webconsole output for various types of objects.

const TEST_URI = "data:text/html;charset=utf8,test for console output - 05";

let dateNow = Date.now();

let inputTests = [
  // 0
  {
    input: "/foo?b*\\s\"ar/igym",
    output: "/foo?b*\\s\"ar/gimy",
    printOutput: "/foo?b*\\s\"ar/gimy",
    inspectable: true,
  },

  // 1
  {
    input: "null",
    output: "null",
  },

  // 2
  {
    input: "undefined",
    output: "undefined",
  },

  // 3
  {
    input: "true",
    output: "true",
  },

  // 4
  {
    input: "new Boolean(false)",
    output: "false",
    inspectable: true,
  },

  // 5
  {
    input: "new Date(" + dateNow + ")",
    output: "Date " + (new Date(dateNow)).toISOString(),
    printOutput: (new Date(dateNow)).toString(),
    inspectable: true,
  },

  // 6
  {
    input: "new Date('test')",
    output: "Invalid Date",
    printOutput: "Invalid Date",
    inspectable: true,
    variablesViewLabel: "Invalid Date",
  },

  // 7
  {
    input: "new Number(43)",
    output: "43",
    inspectable: true,
  },

  // 8
  {
    input: "new String('hello world')",
    output: '"hello world"',
    inspectable: true,
  },
];

function test() {
  Task.spawn(function*() {
    let {tab} = yield loadTab(TEST_URI);
    let hud = yield openConsole(tab);
    return checkOutputForInputs(hud, inputTests);
  }).then(finishTest);
}
