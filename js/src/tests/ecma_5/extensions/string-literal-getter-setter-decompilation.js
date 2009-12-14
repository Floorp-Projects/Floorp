// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

var f;
try
{
  f = eval("(function literalInside() { return { set 'c d e'(v) { } }; })");
}
catch (e)
{
  assertEq(true, false,
           "string-literal property name in setter in object literal in " +
           "function statement failed to parse: " + e);
}

var fstr = "" + f;
assertEq(fstr.indexOf("set") < fstr.indexOf("c d e"), true,
         "should be using new-style syntax with string literal in place of " +
         "property identifier");
assertEq(fstr.indexOf("setter") < 0, true, "using old-style syntax?");

var o = f();
var ostr = "" + o;
assertEq("c d e" in o, true, "missing the property?");
assertEq("set" in Object.getOwnPropertyDescriptor(o, "c d e"), true,
         "'c d e' property not a setter?");
// disabled because we still generate old-style syntax here (toSource
// decompilation is as yet unfixed)
// assertEq(ostr.indexOf("set") < ostr.indexOf("c d e"), true,
//         "should be using new-style syntax when getting the source of a " +
//         "getter/setter while decompiling an object");
// assertEq(ostr.indexOf("setter") < 0, true, "using old-style syntax?");

reportCompare(true, true);
