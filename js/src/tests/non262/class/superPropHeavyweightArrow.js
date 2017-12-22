class foo {
    constructor() { }

    method() {
        return (() => (eval(''), super.toString));
    }
}

assertEq(new foo().method()(), Object.prototype.toString);

if (typeof reportCompare === "function")
    reportCompare(0,0,"OK");
