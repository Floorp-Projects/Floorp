function f1() {
    var o = {};
    var values = [];
    for (var i = 0; i < 6; ++i) {
        var desc = {
            value: i,
            writable: true,
            configurable: true,
            enumerable: true
        };
        try {
            Object.defineProperty(o, "p", desc);
        } catch (e) {
        }
        if (i === 1) {
            Object.defineProperty(o, "p", {configurable: false});
        }
        values.push(o.p);
    }
    assertEq(values.toString(), "0,1,1,1,1,1");
}
f1();

function f2() {
    var o = {};
    for (var i = 0; i < 6; ++i) {
        var desc = {
            value: i,
            writable: true,
            configurable: true,
            enumerable: true
        };
        try {
            Object.defineProperty(o, "p", desc);
        } catch (e) {
        }
        assertEq(Object.getOwnPropertyDescriptor(o, "p").enumerable, true);
        if (i > 0) {
            Object.defineProperty(o, "p", {enumerable: false});
        }
    }
}
f2();
