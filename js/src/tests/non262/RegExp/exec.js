/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var BUGNUMBER = 646490;
var summary =
  "RegExp.prototype.exec doesn't get the lastIndex and ToInteger() it for " +
  "non-global regular expressions when it should";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

function expectThrowTypeError(fun)
{
  try
  {
    var r = fun();
    throw new Error("didn't throw TypeError, returned " + r);
  }
  catch (e)
  {
    assertEq(e instanceof TypeError, true,
             "didn't throw TypeError, got: " + e);
  }
}

function checkExec(description, regex, args, obj)
{
  var lastIndex = obj.lastIndex;
  var index = obj.index;
  var input = obj.input;
  var indexArray = obj.indexArray;

  var res = regex.exec.apply(regex, args);

  assertEq(Array.isArray(res), true, description + ": not an array");
  assertEq(regex.lastIndex, lastIndex, description + ": wrong lastIndex");
  assertEq(res.index, index, description + ": wrong index");
  assertEq(res.input, input, description + ": wrong input");
  assertEq(res.length, indexArray.length, description + ": wrong length");
  for (var i = 0, sz = indexArray.length; i < sz; i++)
    assertEq(res[i], indexArray[i], description + " " + i + ": wrong index value");
}

var exec = RegExp.prototype.exec;
var r, res, called, obj;

/* 1. Let R be this RegExp object. */
expectThrowTypeError(function() { exec.call(null); });
expectThrowTypeError(function() { exec.call(""); });
expectThrowTypeError(function() { exec.call(5); });
expectThrowTypeError(function() { exec.call({}); });
expectThrowTypeError(function() { exec.call([]); });
expectThrowTypeError(function() { exec.call(); });
expectThrowTypeError(function() { exec.call(true); });
expectThrowTypeError(function() { exec.call(Object.create(RegExp.prototype)); });
expectThrowTypeError(function() { exec.call(Object.create(/a/)); });


/* 2. Let S be the value of ToString(string). */
called = false;
r = /a/;
assertEq(r.lastIndex, 0);

checkExec("/a/", r, [{ toString: function() { called = true; return 'ba'; } }],
          { lastIndex: 0,
            index: 1,
            input: "ba",
            indexArray: ["a"] });
assertEq(called, true);

called = false;
try
{
  res = r.exec({ toString: null, valueOf: function() { called = true; throw 17; } });
  throw new Error("didn't throw");
}
catch (e)
{
  assertEq(e, 17);
}

assertEq(called, true);

called = false;
obj = r.lastIndex = { valueOf: function() { assertEq(true, false, "shouldn't have been called"); } };
try
{
  res = r.exec({ toString: null, valueOf: function() { assertEq(called, false); called = true; throw 17; } });
  throw new Error("didn't throw");
}
catch (e)
{
  assertEq(e, 17);
}

assertEq(called, true);
assertEq(r.lastIndex, obj);

// We don't test lack of an argument because of RegExp statics non-standard
// behaviors overriding what really should happen for lack of an argument, sigh.


/*
 * 3. Let length be the length of S.
 * 4. Let lastIndex be the result of calling the [[Get]] internal method of R with argument "lastIndex".
 * 5. Let i be the value of ToInteger(lastIndex).
 */
r = /b/;
r.lastIndex = { valueOf: {}, toString: {} };
expectThrowTypeError(function() { r.exec("foopy"); });
r.lastIndex = { valueOf: function() { throw new TypeError(); } };
expectThrowTypeError(function() { r.exec("foopy"); });


/*
 * 6. Let global be the result of calling the [[Get]] internal method of R with argument "global".
 * 7. If global is false, then let i = 0.
 */
obj = { valueOf: function() { return 5; } };
r = /abc/;
r.lastIndex = obj;

checkExec("/abc/ take one", r, ["abc-------abc"],
          { lastIndex: obj,
            index: 0,
            input: "abc-------abc",
            indexArray: ["abc"] });

checkExec("/abc/ take two", r, ["abc-------abc"],
          { lastIndex: obj,
            index: 0,
            input: "abc-------abc",
            indexArray: ["abc"] });


/*
 * 8. Let matchSucceeded be false.
 * 9. Repeat, while matchSucceeded is false
 *    a. If i < 0 or i > length, then
 *       i. Call the [[Put]] internal method of R with arguments "lastIndex", 0, and true.
 *       ii. Return null.
 *    b. Call the [[Match]] internal method of R with arguments S and i.
 *    c. If [[Match]] returned failure, then
 *       i. Let i = i+1.
 *    d. else
 *       i. Let r be the State result of the call to [[Match]].
 *       ii. Set matchSucceeded to true.
 *    e. Let i = i+1.
 */
r = /abc()?/;
r.lastIndex = -5;
checkExec("/abc()?/ with lastIndex -5", r, ["abc-------abc"],
          { lastIndex: -5,
            index: 0,
            input: "abc-------abc",
            indexArray: ["abc", undefined] });


r = /abc/;
r.lastIndex = -17;
res = r.exec("cdefg");
assertEq(res, null);
assertEq(r.lastIndex, -17);

r = /abc/g;
r.lastIndex = -42;
res = r.exec("cdefg");
assertEq(res, null);
assertEq(r.lastIndex, 0);


/*
 * 10. Let e be r's endIndex value.
 * 11. If global is true,
 *     a. Call the [[Put]] internal method of R with arguments "lastIndex", e, and true.
 */
r = /abc/g;
r.lastIndex = 17;
assertEq(r.exec("sdfs"), null);
assertEq(r.lastIndex, 0);

r = /abc/g;
r.lastIndex = 2;
checkExec("/abc/g", r, ["00abc"],
          { lastIndex: 5,
            index: 2,
            input: "00abc",
            indexArray: ["abc"] });



r = /a(b)c/g;
r.lastIndex = 2;
checkExec("/a(b)c/g take two", r, ["00abcd"],
          { lastIndex: 5,
            index: 2,
            input: "00abcd",
            indexArray: ["abc", "b"] });


/*
 * 12. Let n be the length of r's captures array. (This is the same value as
 *     15.10.2.1's NCapturingParens.)
 * 13. Let A be a new array created as if by the expression new Array() where
 *     Array is the standard built-in constructor with that name.
 * 14. Let matchIndex be the position of the matched substring within the
 *     complete String S.
 * 15. Call the [[DefineOwnProperty]] internal method of A with arguments
 *     "index", Property Descriptor {[[Value]]: matchIndex, [[Writable]: true,
 *     [[Enumerable]]: true, [[Configurable]]: true}, and true.
 * 16. Call the [[DefineOwnProperty]] internal method of A with arguments
 *     "input", Property Descriptor {[[Value]]: S, [[Writable]: true,
 *     [[Enumerable]]: true, [[Configurable]]: true}, and true.
 * 17. Call the [[DefineOwnProperty]] internal method of A with arguments
 *     "length", Property Descriptor {[[Value]]: n + 1}, and true.
 * 18. Let matchedSubstr be the matched substring (i.e. the portion of S
 *     between offset i inclusive and offset e exclusive).
 * 19. Call the [[DefineOwnProperty]] internal method of A with arguments "0",
 *     Property Descriptor {[[Value]]: matchedSubstr, [[Writable]: true,
 *     [[Enumerable]]: true, [[Configurable]]: true}, and true.
 * 20. For each integer i such that I > 0 and I ≤ n
 *     a. Let captureI be i th element of r's captures array.
 *     b. Call the [[DefineOwnProperty]] internal method of A with arguments
 *        ToString(i), Property Descriptor {[[Value]]: captureI, [[Writable]:
 *        true, [[Enumerable]]: true, [[Configurable]]: true}, and true.
 * 21. Return A.
 */
// throughout, above (and in other tests)

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("All tests passed!");
