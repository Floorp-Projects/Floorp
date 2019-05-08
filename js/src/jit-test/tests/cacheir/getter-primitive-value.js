function testNativeGetter() {
    function test() {
        var xs = [Symbol("a"), Symbol("b")];
        var ys = ["a", "b"];

        for (var i = 0; i < 100; ++i) {
            var r = xs[i & 1].description;
            assertEq(r, ys[i & 1]);
        }
    }
    for (var i = 0; i < 2; ++i) test();
}
testNativeGetter();

function testScriptedGetter() {
    Object.defineProperty(Symbol.prototype, "desc", {
        get() {
            "use strict";
            assertEq(typeof this, "symbol");
            return this.description;
        }
    });

    function test() {
        var xs = [Symbol("a"), Symbol("b")];
        var ys = ["a", "b"];

        for (var i = 0; i < 100; ++i) {
            var r = xs[i & 1].desc;
            assertEq(r, ys[i & 1]);
        }
    }
    for (var i = 0; i < 2; ++i) test();
}
testScriptedGetter();

function testScriptedGetterNonStrict() {
    Object.defineProperty(Symbol.prototype, "desc_nonstrict", {
        get() {
            assertEq(typeof this, "object");
            return this.description;
        }
    });

    function test() {
        var xs = [Symbol("a"), Symbol("b")];
        var ys = ["a", "b"];

        for (var i = 0; i < 100; ++i) {
            var r = xs[i & 1].desc_nonstrict;
            assertEq(r, ys[i & 1]);
        }
    }
    for (var i = 0; i < 2; ++i) test();
}
testScriptedGetterNonStrict();

function testNativeGetterPrototype() {
    Object.defineProperty(Object.prototype, "description_proto",
        Object.getOwnPropertyDescriptor(Symbol.prototype, "description"));

    function test() {
        var xs = [Symbol("a"), Symbol("b")];
        var ys = ["a", "b"];

        for (var i = 0; i < 100; ++i) {
            var r = xs[i & 1].description_proto;
            assertEq(r, ys[i & 1]);
        }
    }
    for (var i = 0; i < 2; ++i) test();
}
testNativeGetterPrototype();

function testScriptedGetterPrototype() {
    Object.defineProperty(Object.prototype, "desc_proto", {
        get() {
            "use strict";
            assertEq(typeof this, "symbol");
            return this.description;
        }
    });

    function test() {
        var xs = [Symbol("a"), Symbol("b")];
        var ys = ["a", "b"];

        for (var i = 0; i < 100; ++i) {
            var r = xs[i & 1].desc_proto;
            assertEq(r, ys[i & 1]);
        }
    }
    for (var i = 0; i < 2; ++i) test();
}
testScriptedGetterPrototype();

function testScriptedGetterNonStrictPrototype() {
    Object.defineProperty(Object.prototype, "desc_nonstrict_proto", {
        get() {
            assertEq(typeof this, "object");
            return this.description;
        }
    });

    function test() {
        var xs = [Symbol("a"), Symbol("b")];
        var ys = ["a", "b"];

        for (var i = 0; i < 100; ++i) {
            var r = xs[i & 1].desc_nonstrict_proto;
            assertEq(r, ys[i & 1]);
        }
    }
    for (var i = 0; i < 2; ++i) test();
}
testScriptedGetterNonStrictPrototype();
