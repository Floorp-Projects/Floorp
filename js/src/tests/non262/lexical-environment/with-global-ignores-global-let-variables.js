// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

let v = "global-v";

function f(v, global)
{
  with (global)
    return v;
}

assertEq(f("argument-v", this), "argument-v",
         "let-var shouldn't appear in global for |with| purposes");

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
