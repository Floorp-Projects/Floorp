// |reftest| fails-if(Function("try{Function('let\x20x=5;');return(1,eval)('let\x20x=3;\\'x\\'\x20in\x20this');}catch(e){return(true);}")()) -- needs bug 589199 fix (top-level let not same as var)
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

let v = "global-v";

function f(v, global)
{
  with (global)
    return v;
}

assertEq(f("argument-v", this), "argument-v");

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
