var BUGNUMBER = 1185106;
var summary = "async function constructor and prototype";

print(BUGNUMBER + ": " + summary);

var f1 = async function() {};

var AsyncFunction = f1.constructor;
var AsyncFunctionPrototype = AsyncFunction.prototype;

assertEq(AsyncFunction.name, "AsyncFunction");
assertEq(AsyncFunction.length, 1);
assertEq(Object.getPrototypeOf(async function() {}), AsyncFunctionPrototype);

assertEq(AsyncFunctionPrototype.constructor, AsyncFunction);

var f2 = AsyncFunction("await 1");
assertEq(f2.constructor, AsyncFunction);
assertEq(f2.length, 0);
assertEq(Object.getPrototypeOf(f2), AsyncFunctionPrototype);

var f3 = new AsyncFunction("await 1");
assertEq(f3.constructor, AsyncFunction);
assertEq(f3.length, 0);
assertEq(Object.getPrototypeOf(f3), AsyncFunctionPrototype);

var f4 = AsyncFunction("a", "b", "c", "await 1");
assertEq(f4.constructor, AsyncFunction);
assertEq(f4.length, 3);
assertEq(Object.getPrototypeOf(f4), AsyncFunctionPrototype);

if (typeof reportCompare === "function")
    reportCompare(true, true);
