// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

function innermost() { return arguments.callee.caller; }
function nest() { return eval("innermost();"); }
function nest2() { return nest(); }

assertEq(nest2(), nest);

var innermost = function innermost() { return arguments.callee.caller.caller; };

assertEq(nest2(), nest2);

function nestTwice() { return eval("eval('innermost();');"); }
var nest = nestTwice;

assertEq(nest2(), nest2);

function innermostEval() { return eval("arguments.callee.caller"); }
var innermost = innermostEval;

assertEq(nest2(), nestTwice);

function innermostEvalTwice() { return eval('eval("arguments.callee.caller");'); }
var innermost = innermostEvalTwice;

assertEq(nest2(), nestTwice);

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
