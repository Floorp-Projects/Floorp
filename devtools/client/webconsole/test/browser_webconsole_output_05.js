/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test the webconsole output for various types of objects.

"use strict";

const TEST_URI = "data:text/html;charset=utf8,test for console output - 05";
const ELLIPSIS = Services.prefs.getComplexValue("intl.ellipsis",
  Ci.nsIPrefLocalizedString).data;

// March, 1960: The first implementation of Lisp. From Wikipedia:
//
// > Lisp was first implemented by Steve Russell on an IBM 704 computer. Russell
// > had read McCarthy's paper, and realized (to McCarthy's surprise) that the
// > Lisp eval function could be implemented in machine code. The result was a
// > working Lisp interpreter which could be used to run Lisp programs, or more
// > properly, 'evaluate Lisp expressions.'
var testDate = -310435200000;

var inputTests = [
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
    output: "Boolean { false }",
    printOutput: "false",
    inspectable: true,
    variablesViewLabel: "Boolean { false }"
  },

  // 5
  {
    input: "new Date(" + testDate + ")",
    output: "Date " + (new Date(testDate)).toISOString(),
    printOutput: (new Date(testDate)).toString(),
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
    input: "Date.prototype",
    output: /Object \{.*\}/,
    printOutput: "Invalid Date",
    inspectable: true,
    variablesViewLabel: "Object",
  },

  // 8
  {
    input: "new Number(43)",
    output: "Number { 43 }",
    printOutput: "43",
    inspectable: true,
    variablesViewLabel: "Number { 43 }"
  },

  // 9
  {
    input: "new String('hello')",
    output: /String { "hello", 6 more.* }/,
    printOutput: "hello",
    inspectable: true,
    variablesViewLabel: "String"
  },

  // 10
  {
    input: "(function () { var s = new String('hello'); s.whatever = 23; " +
           " return s;})()",
    output: /String { "hello", whatever: 23, 6 more.* }/,
    printOutput: "hello",
    inspectable: true,
    variablesViewLabel: "String"
  },

  // 11
  {
    input: "(function () { var s = new String('hello'); s[8] = 'x'; " +
           " return s;})()",
    output: /String { "hello", 8: "x", 6 more.* }/,
    printOutput: "hello",
    inspectable: true,
    variablesViewLabel: "String"
  },

  // 12
  {
    // XXX: Can't test fulfilled and rejected promises, because promises get
    // settled on the next tick of the event loop.
    input: "new Promise(function () {})",
    output: 'Promise { <state>: "pending" }',
    printOutput: "[object Promise]",
    inspectable: true,
    variablesViewLabel: "Promise"
  },

  // 13
  {
    input: "(function () { var p = new Promise(function () {}); " +
           "p.foo = 1; return p; }())",
    output: 'Promise { <state>: "pending", foo: 1 }',
    printOutput: "[object Promise]",
    inspectable: true,
    variablesViewLabel: "Promise"
  },

  // 14
  {
    input: "new Object({1: 'this\\nis\\nsupposed\\nto\\nbe\\na\\nvery" +
           "\\nlong\\nstring\\n,shown\\non\\na\\nsingle\\nline', " +
           "2: 'a shorter string', 3: 100})",
    output: 'Object { 1: "this is supposed to be a very long ' + ELLIPSIS +
            '", 2: "a shorter string", 3: 100 }',
    printOutput: "[object Object]",
    inspectable: false,
  },

  // 15
  {
    input: "new Proxy({a:1},[1,2,3])",
    output: 'Proxy { <target>: Object, <handler>: Array[3] }',
    printOutput: "[object Object]",
    inspectable: true,
    variablesViewLabel: "Proxy"
  }
];

function test() {
  requestLongerTimeout(2);
  Task.spawn(function* () {
    let {tab} = yield loadTab(TEST_URI);
    let hud = yield openConsole(tab);
    return checkOutputForInputs(hud, inputTests);
  }).then(finishUp);
}

function finishUp() {
  inputTests = testDate = null;
  finishTest();
}
