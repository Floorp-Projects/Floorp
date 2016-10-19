/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Summary: Ensure typed array validation is called for TypedArray.prototype.map.

const otherGlobal = typeof newGlobal === "function" ? newGlobal() : undefined;
const typedArrayLengths = [0, 1, 1024];

function createTestCases(TAConstructor, constructor, constructorCrossRealm) {
    let testCases = [];
    testCases.push({
        species: constructor,
        method: TAConstructor.prototype.map,
        error: TypeError,
    });
    if (otherGlobal) {
        testCases.push({
            species: constructorCrossRealm,
            method: TAConstructor.prototype.map,
            error: TypeError,
        });
        testCases.push({
            species: constructor,
            method: otherGlobal[TAConstructor.name].prototype.map,
            error: otherGlobal.TypeError,
        });
    }
    return testCases;
}

// Throws TypeError when the returned value is not a typed array.
for (const TAConstructor of anyTypedArrayConstructors) {
    let callCount = 0, expectedCallCount = 0;
    function NoTypedArrayConstructor(...args) {
        let a = [];
        callCount += 1;
        return a;
    }
    function NoTypedArrayConstructorCrossRealm(...args) {
        let a = new otherGlobal.Array();
        callCount += 1;
        return a;
    }
    let testCases = createTestCases(TAConstructor, NoTypedArrayConstructor, NoTypedArrayConstructorCrossRealm);

    for (let {species, method, error} of testCases) {
        for (let length of typedArrayLengths) {
            let ta = new TAConstructor(length);
            ta.constructor = {[Symbol.species]: species};
            assertThrowsInstanceOf(() => method.call(ta, () => 123), error);
            assertEq(callCount, ++expectedCallCount);
        }
    }
}

// Throws TypeError exception when returned array is too small.
for (const TAConstructor of anyTypedArrayConstructors) {
    let callCount = 0, expectedCallCount = 0;
    function TooSmallConstructor(length) {
        let a = new TAConstructor(Math.max(length - 1, 0));
        callCount += 1;
        return a;
    }
    function TooSmallConstructorCrossRealm(length) {
        let a = new otherGlobal[TAConstructor.name](Math.max(length - 1, 0));
        callCount += 1;
        return a;
    }
    let testCases = createTestCases(TAConstructor, TooSmallConstructor, TooSmallConstructorCrossRealm);

    for (let {species, method, error} of testCases) {
        for (let length of typedArrayLengths) {
            let ta = new TAConstructor(length);
            ta.constructor = {[Symbol.species]: species};

            // Passes when the length is zero.
            if (length === 0) {
                let result = method.call(ta, () => 123);
                assertEq(result.length, 0);
            } else {
                assertThrowsInstanceOf(() => method.call(ta, () => 123), error);
            }
            assertEq(callCount, ++expectedCallCount);
        }
    }
}

// No exception when array is larger than requested.
for (const TAConstructor of anyTypedArrayConstructors) {
    const extraLength = 1;

    let callCount = 0, expectedCallCount = 0;
    function TooLargeConstructor(length) {
        let a = new TAConstructor(length + extraLength);
        callCount += 1;
        return a;
    }
    function TooLargeConstructorCrossRealm(length) {
        let a = new otherGlobal[TAConstructor.name](length + extraLength);
        callCount += 1;
        return a;
    }
    let testCases = createTestCases(TAConstructor, TooLargeConstructor, TooLargeConstructorCrossRealm);

    for (let {species, method, error} of testCases) {
        for (let length of typedArrayLengths) {
            let ta = new TAConstructor(length);
            ta.constructor = {[Symbol.species]: species};
            let result = method.call(ta, () => 123);
            assertEq(result.length, length + extraLength);
            assertEq(result[0], (length === 0 ? 0 : 123));
            assertEq(result[length + extraLength - 1], 0);
            assertEq(callCount, ++expectedCallCount);
        }
    }
}

// Throws TypeError exception when returned array is detached.
if (typeof detachArrayBuffer === "function") {
    for (const TAConstructor of typedArrayConstructors) {
        let callCount = 0, expectedCallCount = 0;
        function DetachConstructor(...args) {
            let a = new TAConstructor(...args);
            detachArrayBuffer(a.buffer);
            callCount += 1;
            return a;
        }
        function DetachConstructorCrossRealm(...args) {
            let a = new otherGlobal[TAConstructor.name](...args);
            otherGlobal.detachArrayBuffer(a.buffer);
            callCount += 1;
            return a;
        }
        let testCases = createTestCases(TAConstructor, DetachConstructor, DetachConstructorCrossRealm);

        for (let {species, method, error} of testCases) {
            for (let length of typedArrayLengths) {
                let ta = new TAConstructor(length);
                ta.constructor = {[Symbol.species]: species};
                assertThrowsInstanceOf(() => method.call(ta, () => 123), error);
                assertEq(callCount, ++expectedCallCount);
            }
        }
    }
}

if (typeof reportCompare === "function")
    reportCompare(0, 0);
