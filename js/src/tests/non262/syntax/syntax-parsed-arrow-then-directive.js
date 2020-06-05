/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

//-----------------------------------------------------------------------------
var BUGNUMBER = 1596706;
var summary =
  "Properly apply a directive comment that's only tokenized by a syntax " +
  "parser (because the directive comment appears immediately after an arrow " +
  "function with expression body)";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var stack;

function reset()
{
  stack = "";
}

function assertStackContains(needle, msg)
{
  assertEq(stack.indexOf(needle) >= 0, true,
           `stack should contain '${needle}': ${msg}`);
}

Object.defineProperty(this, "detectSourceURL", {
  get() {
    stack = new Error().stack;
    return 17;
  }
});

// block followed by semicolon
reset();
assertEq(eval(`x=>{};
//# sourceURL=http://example.com/foo.js
detectSourceURL`), 17);
assertStackContains("http://example.com/foo.js", "block, semi");

// block not followed by semicolon
reset();
assertEq(eval(`x=>{}
//# sourceURL=http://example.com/bar.js
detectSourceURL`), 17);
assertStackContains("http://example.com/bar.js", "block, not semi");

// expr followed by semicolon
reset();
assertEq(eval(`x=>y;
//# sourceURL=http://example.com/baz.js
detectSourceURL`), 17);
assertStackContains("http://example.com/baz.js", "expr, semi");

// expr not followed by semicolon
reset();
assertEq(eval(`x=>y
//# sourceURL=http://example.com/quux.js
detectSourceURL`), 17);
assertStackContains("http://example.com/quux.js", "expr, not semi");

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
