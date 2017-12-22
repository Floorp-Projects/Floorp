// basic static method test
class X {
    static count() { return ++this.hits; }
    constructor() { }
}
X.hits = 0;
assertEq(X.count(), 1);

// A static method is just a function.
assertEq(X.count instanceof Function, true);
assertEq(X.count.length, 0);
assertEq(X.count.bind({hits: 77})(), 78);

if (typeof reportCompare === "function")
    reportCompare(0, 0, "OK");
