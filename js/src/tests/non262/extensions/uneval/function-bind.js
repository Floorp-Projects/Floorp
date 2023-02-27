// |reftest| skip-if(!this.uneval)

/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var gTestfile = 'function-bind.js';
var BUGNUMBER = 429507;
var summary = "ES5: Function.prototype.bind";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

assertEq((function unbound(){"body"}).bind().toSource(), `function bound() {
    [native code]
}`);

assertEq(uneval((function unbound(){"body"}).bind()), `function bound() {
    [native code]
}`);


/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("All tests passed!");
