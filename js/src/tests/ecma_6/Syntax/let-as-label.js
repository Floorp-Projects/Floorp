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
assertThrowsInstanceOf(() => eval(" 'use strict' \n let: 42;"), SyntaxError);

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
