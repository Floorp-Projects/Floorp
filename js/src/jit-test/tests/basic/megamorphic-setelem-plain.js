setJitCompilerOption("ic.force-megamorphic", 1);

// The megamorphic SetElem.
function doSet(obj, prop, v) {
    "use strict";
    obj[prop] = v;
}
function test() {
    with ({}) {} // No inlining.
    for (var i = 0; i < 10; i++) {
        var obj = {};

        // Test simple add/set cases.
        for (var j = 0; j < 10; j++) {
            doSet(obj, "x" + (j % 8), j);
        }

        // Ensure __proto__ is handled correctly.
        var setterCalls = 0;
        var proto = {set someSetter(v) { setterCalls++; }};
        doSet(obj, "__proto__", proto);
        assertEq(Object.getPrototypeOf(obj), proto);

        // Can't shadow a non-writable data property.
        Object.defineProperty(proto, "readonly",
                              {value: 1, writable: false, configurable: true});
        var ex = null;
        try {
            doSet(obj, "readonly", 2);
        } catch (e) {
            ex = e;
        }
        assertEq(ex instanceof TypeError, true);
        assertEq(obj.readonly, 1);

        // Setter on the proto chain must be called.
        doSet(obj, "someSetter", 1);
        assertEq(setterCalls, 1);

        // Can't add properties if non-extensible.
        Object.preventExtensions(obj);
        ex = null;
        try {
            doSet(obj, "foo", 1);
        } catch (e) {
            ex = e;
        }
        assertEq(ex instanceof TypeError, true);

        assertEq(JSON.stringify(obj), '{"x0":8,"x1":9,"x2":2,"x3":3,"x4":4,"x5":5,"x6":6,"x7":7}');
    }
}
test();
