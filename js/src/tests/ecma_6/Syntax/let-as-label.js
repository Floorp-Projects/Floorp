/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

//-----------------------------------------------------------------------------
var BUGNUMBER = 1288459;
var summary = "let can't be used as a label in strict mode code";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

Function("let: 42")
assertThrowsInstanceOf(() => Function(" 'use strict'; let: 42"), SyntaxError);
assertThrowsInstanceOf(() => Function(" 'use strict' \n let: 42"), SyntaxError);

eval("let: 42")
assertThrowsInstanceOf(() => eval(" 'use strict'; let: 42"), SyntaxError);

// XXX This will be fixed later in bug 1288459's patch stack.
assertThrowsInstanceOf(() => eval(" 'use strict' \n let: 42; /*XXX*/throw new SyntaxError();"), SyntaxError);

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
