// Test source location for missing-super-call check at the end of a derived class constructor.
class A {};
class B extends A {
    constructor(x) {
        if (x === null) {
            throw "fail";
        }
    }
};
let ex;
try {
    new B();
} catch (e) {
    ex = e;
}
assertEq(ex instanceof ReferenceError, true);
// The closing '}' of B's constructor.
assertEq(ex.lineNumber, 8);
assertEq(ex.columnNumber, 5);
