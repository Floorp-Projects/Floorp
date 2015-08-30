// |reftest| fails-if(Function("try{Function('let\x20x=5;');return(evaluate)('let\x20x=3;\\'x\\'\x20in\x20this');}catch(e){return(true);}")()) -- needs bug 589199 fix (top-level let not same as var); please convert AssertEq to assertEq when removing this fails-if, too
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

let v = "global-v";

function f(v, global)
{
  with (global)
    return v;
}

// Don't use assertEq because it triggers tbpl error-highlighting false
// positives.  When this test isn't fails-if, just use assertEq directly.
var AssertEq = typeof reportCompare === "function"
             ? (act, exp, msg) => reportCompare(exp, act, msg)
             : assertEq;

AssertEq(f("argument-v", this), "argument-v",
         "let-var shouldn't appear in global for |with| purposes");

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
