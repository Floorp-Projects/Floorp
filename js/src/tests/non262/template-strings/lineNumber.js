/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/ */

var BUGNUMBER = 1253847;
var summary = 'Line numbers should be updated correctly when a template literal contains CRLF sequences.';

print(BUGNUMBER + ": " + summary);

// lineNumber after template literal.
var base, x, lineno;
eval([
  "base = new Error().lineNumber;",
  "x = `",
  "`;",
  "lineno = new Error().lineNumber;",
].join("\r\n"));
assertEq(lineno, base + 3);

eval([
  "base = new Error().lineNumber;",
  "x = `",
  "",
  "`;",
  "lineno = new Error().lineNumber;",
].join("\r\n"));
assertEq(lineno, base + 4);

eval([
  "base = new Error().lineNumber;",
  "x = `a",
  "b`;",
  "lineno = new Error().lineNumber;",
].join("\r\n"));
assertEq(lineno, base + 3);

// lineNumber inside template literal
eval([
  "base = new Error().lineNumber;",
  "x = `${new Error().lineNumber}",
  "${new Error().lineNumber}",
  "${new Error().lineNumber}`;",
  "lineno = new Error().lineNumber;",
].join("\r\n"));
assertEq(lineno, base + 4);
assertEq(x, (base + 1) + '\n' + (base + 2) + '\n' + (base + 3));

eval([
  "var base = new Error().lineNumber;",
  "var x = `",
  "${new Error().lineNumber}",
  "${new Error().lineNumber}",
  "${new Error().lineNumber}",
  "`;",
  "var lineno = new Error().lineNumber;",
].join("\r\n"));
assertEq(lineno, base + 6);
assertEq(x, '\n' + (base + 2) + '\n' + (base + 3) + '\n' + (base + 4) + '\n');

// When CR, LF, and CRLF are mixed.
eval([
  "var base = new Error().lineNumber;", "\n",
  "var x = `${new Error().lineNumber}", "\n",
  "${new Error().lineNumber}", "\r\n",
  "${new Error().lineNumber}", "\r",
  "${new Error().lineNumber}`;", "\n",
  "var lineno = new Error().lineNumber;",
].join(""));
assertEq(lineno, base + 5);
assertEq(x, (base + 1) + '\n' + (base + 2) + '\n' + (base + 3) + '\n' + (base + 4));

if (typeof reportCompare === "function")
  reportCompare(true, true);
