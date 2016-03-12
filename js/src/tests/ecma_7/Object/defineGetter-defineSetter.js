/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

let count = 0;
let verifyProxy = new Proxy({}, {
    defineProperty(target, property, descriptor) {
        assertEq(property, "x");

        assertEq(descriptor.enumerable, true);
        assertEq(descriptor.configurable, true);

        if ("set" in descriptor)
            assertEq(descriptor.set, Object.prototype.__defineSetter__);
        else
            assertEq(descriptor.get, Object.prototype.__defineGetter__);

        assertEq(Object.keys(descriptor).length, 3);

        count++;
        return true;
    }
});

for (let define of [Object.prototype.__defineGetter__, Object.prototype.__defineSetter__]) {
    // null/undefined |this| value
    for (let thisv of [undefined, null])
        assertThrowsInstanceOf(() => define.call(thisv, "x", define), TypeError);

    // non-callable getter/setter
    let nonCallable = [{}, [], new Proxy({}, {})];
    for (let value of nonCallable)
        assertThrowsInstanceOf(() => define.call(verifyProxy, "x", value), TypeError);

    // ToPropertyKey
    let key = {
        [Symbol.toPrimitive](hint) {
            assertEq(hint, "string");
            // Throws, because non-primitive is returned
            return {};
        },
        valueOf() { throw "wrongly invoked"; },
        toString() { throw "wrongly invoked"; }
    };
    assertThrowsInstanceOf(() => define.call(verifyProxy, key, define), TypeError);

    key = {
        [Symbol.toPrimitive](hint) {
            assertEq(hint, "string");
            return "x";
        },
        valueOf() { throw "wrongly invoked"; },
        toString() { throw "wrongly invoked"; }
    }
    define.call(verifyProxy, key, define);

    key = {
        [Symbol.toPrimitive]: undefined,

        valueOf() { throw "wrongly invoked"; },
        toString() { return "x"; }
    }
    define.call(verifyProxy, key, define);

    // Bog standard call
    define.call(verifyProxy, "x", define);

    let obj = {};
    define.call(obj, "x", define);
    let descriptor = Object.getOwnPropertyDescriptor(obj, "x");

    assertEq(descriptor.enumerable, true);
    assertEq(descriptor.configurable, true);

    if (define == Object.prototype.__defineSetter__) {
        assertEq(descriptor.get, undefined);
        assertEq(descriptor.set, define);
    } else {
        assertEq(descriptor.get, define);
        assertEq(descriptor.set, undefined);
    }

    assertEq(Object.keys(descriptor).length, 4);


}

// Number of calls that should succeed
assertEq(count, 6);

reportCompare(0, 0);
