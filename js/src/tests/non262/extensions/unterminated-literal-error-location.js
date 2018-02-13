/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var BUGNUMBER = 1434429;
var summary =
  "Report unterminated string/template literal errors with the line/column " +
  "number of the point of non-termination";

function test(f, quotes, [line, col])
{
  var caught = false;
  try
  {
    f();
  }
  catch (e)
  {
    caught = true;
    assertEq(e.lineNumber, line, "line number");
    assertEq(e.columnNumber, col, "column number");
    assertEq(e.message.includes(quotes), true,
             "message must contain delimiter");
  }

  assertEq(caught, true);
}

test(function() {
      //0123
  eval("'hi");
}, "''", [1, 3]);

test(function() {
      //0123 4
  eval("'hi\\");
}, "''", [1, 4]);

test(function() {
      //0123456
  eval("   'hi");
}, "''", [1, 6]);

test(function() {
      //0123456 7
  eval("   'hi\\");
}, "''", [1, 7]);

test(function() {
      //01234567 01234567
  eval('var x =\n    "hi');
}, '""', [2, 7]);

test(function() {
      //0123456  01234567 8
  eval('var x =\n    "hi\\');
}, '""', [2, 8]);

test(function() {
      //                                          1
      //0123456  01234567   012345678   01234567890123
  eval('var x =\n    "hi\\\n     bye\\\n    no really');
}, '""', [4, 13]);

test(function() {
      //                                          1
      //0123456  01234567   012345678   01234567890123 4
  eval('var x =\n    "hi\\\n     bye\\\n    no really\\');
}, '""', [4, 14]);

test(function() {
      //0123456  01234567   012345678
  eval('var x =\n    "hi\\\n     bye\n');
}, '""', [3, 8]);

test(function() {
      //0123456  01234567   012345678 9
  eval('var x =\n    "hi\\\n     bye\\');
}, '""', [3, 9]);

test(function() {
      //0123456  01234567
  eval('var x =\n      `');
}, '``', [2, 7]);

test(function() {
      //0123456  01234567 8
  eval('var x =\n      `\\');
}, '``', [2, 8]);

test(function() {
      //                   1
      //0123456  0123456789012345
  eval('var x =\n    htmlEscape`');
}, '``', [2, 15]);

test(function() {
      //                   1
      //0123456  0123456789012345 6
  eval('var x =\n    htmlEscape`\\');
}, '``', [2, 16]);

test(function() {
      //                   1
      //0123456  01234567890123  01234
  eval('var x =\n    htmlEscape\n   `');
}, '``', [3, 4]);

test(function() {
      //                   1
      //0123456  01234567890123  01234 5
  eval('var x =\n    htmlEscape\n   `\\');
}, '``', [3, 5]);

if (typeof reportCompare === "function")
  reportCompare(0, 0, "ok");

print("Tests complete");
