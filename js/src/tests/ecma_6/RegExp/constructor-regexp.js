var BUGNUMBER = 1130860;
var summary = "RegExp constructor shouldn't invoke flags getter on argument RegExp instance.";

print(BUGNUMBER + ": " + summary);

// same-compartment
var a = /foo/;
var flagsCalled = false;
Object.defineProperty(a, "flags", { get: () => {
  flagsCalled = true;
  return "i";
}});

assertEq(a.flags, "i");
assertEq(flagsCalled, true);

flagsCalled = false;
assertEq(new RegExp(a).flags, "");
assertEq(flagsCalled, false);

// cross-compartment
var g = newGlobal();
var b = g.eval(`
var b = /foo2/;
var flagsCalled = false;
Object.defineProperty(b, "flags", { get: () => {
  flagsCalled = true;
  return "i";
}});
b;
`);

assertEq(b.flags, "i");
assertEq(g.eval("flagsCalled;"), true);

g.eval(`
flagsCalled = false;
`);
assertEq(new RegExp(b).flags, "");
assertEq(g.eval("flagsCalled;"), false);

if (typeof reportCompare === "function")
    reportCompare(true, true);
