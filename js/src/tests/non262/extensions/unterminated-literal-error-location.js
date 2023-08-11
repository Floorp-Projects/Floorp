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
      //1234
  eval("'hi");
}, "''", [1, 4]);

test(function() {
      //1234 5
  eval("'hi\\");
}, "''", [1, 5]);

test(function() {
      //1234567
  eval("   'hi");
}, "''", [1, 7]);

test(function() {
      //1234567 8
  eval("   'hi\\");
}, "''", [1, 8]);

test(function() {
      //12345678 12345678
  eval('var x =\n    "hi');
}, '""', [2, 8]);

test(function() {
      //1234567  12345678 9
  eval('var x =\n    "hi\\');
}, '""', [2, 9]);

test(function() {
      //                                         1
      //1234567  12345678   123456789   12345678901234
  eval('var x =\n    "hi\\\n     bye\\\n    no really');
}, '""', [4, 14]);

test(function() {
      //                                         1
      //1234567  12345678   123456789   12345678901234 5
  eval('var x =\n    "hi\\\n     bye\\\n    no really\\');
}, '""', [4, 15]);

test(function() {
      //1234567  12345678   123456789
  eval('var x =\n    "hi\\\n     bye\n');
}, '""', [3, 9]);

test(function() {
      //                              1
      //1234567  12345678   123456789 0
  eval('var x =\n    "hi\\\n     bye\\');
}, '""', [3, 10]);

test(function() {
      //1234567  12345678
  eval('var x =\n      `');
}, '``', [2, 8]);

test(function() {
      //1234567  12345678 9
  eval('var x =\n      `\\');
}, '``', [2, 9]);

test(function() {
      //                  1
      //1234567  1234567890123456
  eval('var x =\n    htmlEscape`');
}, '``', [2, 16]);

test(function() {
      //                  1
      //1234567  1234567890123456 7
  eval('var x =\n    htmlEscape`\\');
}, '``', [2, 17]);

test(function() {
      //                  1
      //1234567  12345678901234  12345
  eval('var x =\n    htmlEscape\n   `');
}, '``', [3, 5]);

test(function() {
      //                  1
      //1234567  12345678901234  12345 6
  eval('var x =\n    htmlEscape\n   `\\');
}, '``', [3, 6]);

if (typeof reportCompare === "function")
  reportCompare(0, 0, "ok");

print("Tests complete");
