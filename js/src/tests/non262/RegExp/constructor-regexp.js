var BUGNUMBER = 1130860;
var summary = "RegExp constructor shouldn't invoke source/flags getters on argument RegExp instance.";

print(BUGNUMBER + ": " + summary);

// same-compartment
var a = /foo/;
var flagsCalled = false;
var sourceCalled = false;
Object.defineProperty(a, "source", { get: () => {
  sourceCalled = true;
  return "bar";
}});
Object.defineProperty(a, "flags", { get: () => {
  flagsCalled = true;
  return "i";
}});

assertEq(a.source, "bar");
assertEq(a.flags, "i");
assertEq(sourceCalled, true);
assertEq(flagsCalled, true);

sourceCalled = false;
flagsCalled = false;
assertEq(new RegExp(a).source, "foo");
assertEq(sourceCalled, false);
assertEq(flagsCalled, false);

// cross-compartment
var g = newGlobal();
var b = g.eval(`
var b = /foo2/;
var flagsCalled = false;
var sourceCalled = false;
Object.defineProperty(b, "source", { get: () => {
  sourceCalled = true;
  return "bar2";
}});
Object.defineProperty(b, "flags", { get: () => {
  flagsCalled = true;
  return "i";
}});
b;
`);

assertEq(b.source, "bar2");
assertEq(b.flags, "i");
assertEq(g.eval("sourceCalled;"), true);
assertEq(g.eval("flagsCalled;"), true);

g.eval(`
sourceCalled = false;
flagsCalled = false;
`);
assertEq(new RegExp(b).source, "foo2");
assertEq(g.eval("sourceCalled;"), false);
assertEq(g.eval("flagsCalled;"), false);

if (typeof reportCompare === "function")
    reportCompare(true, true);
