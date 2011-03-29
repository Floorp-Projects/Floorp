/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var BUGNUMBER = 640072;
var summary =
  "Represent /a/.{lastIndex,global,source,multiline,sticky,ignoreCase} with " +
  "plain old data properties";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

function checkDataProperty(obj, p, expect, msg)
{
  var d = Object.getOwnPropertyDescriptor(obj, p);

  assertEq(d.value, expect.value, msg + ": bad value for " + p);
  assertEq(d.writable, expect.writable, msg + ": bad writable for " + p);
  assertEq(d.enumerable, expect.enumerable, msg + ": bad enumerable for " + p);
  assertEq(d.configurable, expect.configurable, msg + ": bad configurable for " + p);

  // Try redefining the property using its initial values: these should all be
  // silent no-ops.
  Object.defineProperty(obj, p, { value: expect.value });
  Object.defineProperty(obj, p, { writable: expect.writable });
  Object.defineProperty(obj, p, { enumerable: expect.enumerable });
  Object.defineProperty(obj, p, { configurable: expect.configurable });

  var d2 = Object.getOwnPropertyDescriptor(obj, p);
  assertEq(d.value, d2.value, msg + ": value changed on redefinition of " + p + "?");
  assertEq(d.writable, d2.writable, msg + ": writable changed on redefinition of " + p + "?");
  assertEq(d.enumerable, d2.enumerable, msg + ": enumerable changed on redefinition of " + p + "?");
  assertEq(d.configurable, d2.configurable, msg + ": configurable changed on redefinition of " + p + "?");
}


// Check a bunch of "empty" regular expressions first.

var choices = [{ msg: "RegExp.prototype",
                 get: function() { return RegExp.prototype; } },
               { msg: "new RegExp()",
                 get: function() { return new RegExp(); } },
               { msg: "/(?:)/",
                 get: Function("return /(?:)/;") }];

function checkRegExp(r, msg, lastIndex, global, ignoreCase, multiline)
{
  var expect;

  expect = { value: lastIndex, enumerable: false, configurable: false, writable: true };
  checkDataProperty(r, "lastIndex", expect, msg);

  // check source specially: its value is under-defined in the spec
  var d = Object.getOwnPropertyDescriptor(r, "source");
  assertEq(d.writable, false, "bad writable: " + msg);
  assertEq(d.enumerable, false, "bad enumerable: " + msg);
  assertEq(d.configurable, false, "bad configurable: " + msg);

  expect = { value: global, enumerable: false, configurable: false, writable: false };
  checkDataProperty(r, "global", expect, msg);

  expect = { value: ignoreCase, enumerable: false, configurable: false, writable: false };
  checkDataProperty(r, "ignoreCase", expect, msg);

  expect = { value: multiline, enumerable: false, configurable: false, writable: false };
  checkDataProperty(r, "multiline", expect, msg);
}

checkRegExp(RegExp.prototype, "RegExp.prototype", 0, false, false, false);
checkRegExp(new RegExp(), "new RegExp()", 0, false, false, false);
checkRegExp(/(?:)/, "/(?:)/", 0, false, false, false);
checkRegExp(Function("return /(?:)/;")(), 'Function("return /(?:)/;")()', 0, false, false, false);

for (var i = 0; i < choices.length; i++)
{
  var choice = choices[i];
  var msg = choice.msg;
  var r = choice.get();

  checkRegExp(r, msg, 0, false, false, false);
}

// Now test less generic regular expressions

checkRegExp(/a/gim, "/a/gim", 0, true, true, true);

var r;

do
{
  r = /abcd/mg;
  checkRegExp(r, "/abcd/mg initially", 0, true, false, true);
  r.exec("abcdefg");
  checkRegExp(r, "/abcd/mg step 1", 4, true, false, true);
  r.exec("abcdabcd");
  checkRegExp(r, "/abcd/mg step 2", 8, true, false, true);
  r.exec("abcdabcd");
  checkRegExp(r, "/abcd/mg end", 0, true, false, true);

  r = /cde/ig;
  checkRegExp(r, "/cde/ig initially", 0, true, true, false);
  var obj = r.lastIndex = { valueOf: function() { return 2; } };
  checkRegExp(r, "/cde/ig after lastIndex", obj, true, true, false);
  r.exec("aaacdef");
  checkRegExp(r, "/cde/ig after exec", 6, true, true, false);
  Object.defineProperty(r, "lastIndex", { value: 3 });
  checkRegExp(r, "/cde/ig after define 3", 3, true, true, false);
  Object.defineProperty(r, "lastIndex", { value: obj });
  checkRegExp(r, "/cde/ig after lastIndex", obj, true, true, false);


  // Tricky bits of testing: make sure that redefining lastIndex doesn't change
  // the slot where the lastIndex property is initially stored, even if
  // the redefinition also changes writability.
  r = /a/g;
  checkRegExp(r, "/a/g initially", 0, true, false, false);
  Object.defineProperty(r, "lastIndex", { value: 2 });
  r.exec("aabbbba");
  checkRegExp(r, "/a/g after first exec", 7, true, false, false);
  assertEq(r.lastIndex, 7);
  r.lastIndex = 2;
  checkRegExp(r, "/a/g after assign", 2, true, false, false);
  r.exec("aabbbba");
  assertEq(r.lastIndex, 7); // check in reverse order
  checkRegExp(r, "/a/g after second exec", 7, true, false, false);

  r = /c/g;
  r.lastIndex = 2;
  checkRegExp(r, "/c/g initially", 2, true, false, false);
  Object.defineProperty(r, "lastIndex", { writable: false });
  assertEq(Object.getOwnPropertyDescriptor(r, "lastIndex").writable, false);
  try { r.exec("aabbbba"); } catch (e) { /* swallow error if thrown */ }
  assertEq(Object.getOwnPropertyDescriptor(r, "lastIndex").writable, false);
  assertEq(Object.getOwnPropertyDescriptor(r, "source").writable, false);
  assertEq(Object.getOwnPropertyDescriptor(r, "global").value, true);
  assertEq(Object.getOwnPropertyDescriptor(r, "global").writable, false);
  assertEq(Object.getOwnPropertyDescriptor(r, "ignoreCase").value, false);
  assertEq(Object.getOwnPropertyDescriptor(r, "ignoreCase").writable, false);
  assertEq(Object.getOwnPropertyDescriptor(r, "multiline").value, false);
  assertEq(Object.getOwnPropertyDescriptor(r, "multiline").writable, false);
}
while (Math.random() > 17); // fake loop to discourage RegExp object caching

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("All tests passed!");
