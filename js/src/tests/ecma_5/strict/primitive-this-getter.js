let primitives = [
    10,
    false,
    "test",
    Symbol()
]

let getter = "getter";
let getter2 = "getter2";
let key = "key";

for (let value of primitives) {
    let prototype = Object.getPrototypeOf(value);

    // Strict getters receive a primitive this value.
    Object.defineProperty(prototype, "getter", {get: function() {
        "use strict";
        assertEq(this, value);
        return "getter";
    }})

    assertEq(value.getter, "getter");
    assertEq(value[getter], "getter");

    // The proxy's [[Get]] trap is also invoked with primitive receiver values.
    let proxy = new Proxy({}, {
        get(target, property, receiver) {
            assertEq(property, "key");
            assertEq(receiver, value);
            return "get";
        }
    });

    Object.setPrototypeOf(prototype, proxy);
    assertEq(value.key, "get");
    assertEq(value[key], "get");
    assertEq(value.getter, "getter");
    assertEq(value[getter], "getter");

    // A getter still gets a primitive this value even after going through a proxy.
    proxy = new Proxy({
        get getter2() {
            "use strict";
            assertEq(this, value);
            return "getter2";
        }
    }, {});

    Object.setPrototypeOf(prototype, proxy);
    assertEq(value.getter2, "getter2");
    assertEq(value[getter2], "getter2");
    assertEq(value.getter, "getter");
    assertEq(value[getter], "getter");
}

reportCompare(true, true);
