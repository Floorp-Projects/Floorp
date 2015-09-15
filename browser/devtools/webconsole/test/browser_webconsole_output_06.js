 /* vim: set ft=javascript ts=2 et sw=2 tw=80: */
 /* Any copyright is dedicated to the Public Domain.
  http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the webconsole output for various arrays.

const TEST_URI = "data:text/html;charset=utf8,test for console output - 06";
const ELLIPSIS = Services.prefs.getComplexValue("intl.ellipsis",
  Ci.nsIPrefLocalizedString).data;
const testStrIn = "SHOW\\nALL\\nOF\\nTHIS\\nON\\nA\\nSINGLE" +
                  "\\nLINE ONLY. ESCAPE ALL NEWLINE";
const testStrOut = "SHOW ALL OF THIS ON A SINGLE LINE O" + ELLIPSIS;

var inputTests = [
  // 1 - array with empty slots only
  {
    input: "Array(5)",
    output: "Array [ <5 empty slots> ]",
    printOutput: ",,,,",
    inspectable: true,
    variablesViewLabel: "Array[5]",
  },
  // 2 - array with one empty slot at the beginning
  {
    input: "[,1,2,3]",
    output: "Array [ <1 empty slot>, 1, 2, 3 ]",
    printOutput: ",1,2,3",
    inspectable: true,
    variablesViewLabel: "Array[4]",
  },
  // 3 - array with multiple consecutive empty slots at the beginning
  {
    input: "[,,,3,4,5]",
    output: "Array [ <3 empty slots>, 3, 4, 5 ]",
    printOutput: ",,,3,4,5",
    inspectable: true,
    variablesViewLabel: "Array[6]",
  },
  // 4 - array with one empty slot at the middle
  {
    input: "[0,1,,3,4,5]",
    output: "Array [ 0, 1, <1 empty slot>, 3, 4, 5 ]",
    printOutput: "0,1,,3,4,5",
    inspectable: true,
    variablesViewLabel: "Array[6]",
  },
  // 5 - array with multiple successive empty slots at the middle
  {
    input: "[0,1,,,,5]",
    output: "Array [ 0, 1, <3 empty slots>, 5 ]",
    printOutput: "0,1,,,,5",
    inspectable: true,
    variablesViewLabel: "Array[6]",
  },
  // 6 - array with multiple non successive single empty slots
  {
    input: "[0,,2,,4,5]",
    output: "Array [ 0, <1 empty slot>, 2, <1 empty slot>, 4, 5 ]",
    printOutput: "0,,2,,4,5",
    inspectable: true,
    variablesViewLabel: "Array[6]",
  },
  // 7 - array with multiple multi-slot holes
  {
    input: "[0,,,3,,,,7,8]",
    output: "Array [ 0, <2 empty slots>, 3, <3 empty slots>, 7, 8 ]",
    printOutput: "0,,,3,,,,7,8",
    inspectable: true,
    variablesViewLabel: "Array[9]",
  },
  // 8 - array with a single slot hole at the end
  {
    input: "[0,1,2,3,4,,]",
    output: "Array [ 0, 1, 2, 3, 4, <1 empty slot> ]",
    printOutput: "0,1,2,3,4,",
    inspectable: true,
    variablesViewLabel: "Array[6]",
  },
  // 9 - array with multiple consecutive empty slots at the end
  {
    input: "[0,1,2,,,,]",
    output: "Array [ 0, 1, 2, <3 empty slots> ]",
    printOutput: "0,1,2,,,",
    inspectable: true,
    variablesViewLabel: "Array[6]",
  },

  // 10 - array with members explicitly set to null
  {
    input: "[0,null,null,3,4,5]",
    output: "Array [ 0, null, null, 3, 4, 5 ]",
    printOutput: "0,,,3,4,5",
    inspectable: true,
    variablesViewLabel: "Array[6]"
  },

  // 11 - array with members explicitly set to undefined
  {
    input: "[0,undefined,undefined,3,4,5]",
    output: "Array [ 0, undefined, undefined, 3, 4, 5 ]",
    printOutput: "0,,,3,4,5",
    inspectable: true,
    variablesViewLabel: "Array[6]"
  },

  // 12 - array with long strings as elements
  {
    input: '["' + testStrIn + '", "' + testStrIn + '", "' + testStrIn + '"]',
    output: 'Array [ "' + testStrOut + '", "' + testStrOut + '", "' +
            testStrOut + '" ]',
    inspectable: false,
    printOutput: "SHOW\nALL\nOF\nTHIS\nON\nA\nSINGLE\nLINE ONLY. ESCAPE " +
                 "ALL NEWLINE,SHOW\nALL\nOF\nTHIS\nON\nA\nSINGLE\nLINE ONLY. " +
                 "ESCAPE ALL NEWLINE,SHOW\nALL\nOF\nTHIS\nON\nA\nSINGLE\n" +
                 "LINE ONLY. ESCAPE ALL NEWLINE",
    variablesViewLabel: "Array[3]"
  },

  // 13
  {
    input: '({0: "a", 1: "b"})',
    output: 'Object [ "a", "b" ]',
    printOutput: "[object Object]",
    inspectable: false,
  },

  // 14
  {
    input: '({0: "a", 42: "b"})',
    output: 'Object { 0: "a", 42: "b" }',
    printOutput: "[object Object]",
    inspectable: false,
  },

  // 15
  {
    input: '({0: "a", 1: "b", 2: "c", 3: "d", 4: "e", 5: "f", 6: "g", ' +
           '7: "h", 8: "i", 9: "j", 10: "k", 11: "l"})',
    output: 'Object [ "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", ' +
            '2 more\u2026 ]',
    printOutput: "[object Object]",
    inspectable: true,
    variablesViewLabel: "Object[12]",
  },

  // 16
  {
    input: '({0: "a", 1: "b", 2: "c", 3: "d", 4: "e", 5: "f", 6: "g", ' +
           '7: "h", 8: "i", 9: "j", 10: "k", 11: "l", m: "n"})',
    output: 'Object { 0: "a", 1: "b", 2: "c", 3: "d", 4: "e", 5: "f", ' +
            '6: "g", 7: "h", 8: "i", 9: "j", 3 more\u2026 }',
    printOutput: "[object Object]",
    inspectable: true,
    variablesViewLabel: "Object",
  },
];

function test() {
  requestLongerTimeout(2);
  Task.spawn(function*() {
    let {tab} = yield loadTab(TEST_URI);
    let hud = yield openConsole(tab);
    return checkOutputForInputs(hud, inputTests);
  }).then(finishUp);
}

function finishUp() {
  inputTests = null;
  finishTest();
}
