foo = 1;
Object.defineProperty(this, "foo", {writable:false});
foo = 2;
assertEq(foo, 1);

if (typeof reportCompare === "function")
    reportCompare(true, true);
