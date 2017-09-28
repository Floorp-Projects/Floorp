var called = false;
var a = [/* hole */, undefined, {
    toString() {
        if (!called) {
            called = true;
            a.length = 3;
            Object.defineProperty(a, "length", {writable:false});
        }
        return 0;
    }
}, 0];
a.sort();

assertEq(a.length, 3);
assertEq(a[1], 0);
assertEq(a[2], undefined);
