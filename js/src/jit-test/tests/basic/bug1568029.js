function TestObject(a) {
    this.a = 1;
    if (a >= 0) {
        this.b = 2;
    }

    if (a > 0) {
        new TestObject(a - 1);
    }
    if (a == 0) {
        markObjectPropertiesUnknown(this);
    }
}

// Force analysis. There may be a better way.
for (let i = 0; i < 1000; i++) {
    new TestObject(-1);
}

let x = new TestObject(1);
assertEq(x.a, 1);
assertEq(x.b, 2);
