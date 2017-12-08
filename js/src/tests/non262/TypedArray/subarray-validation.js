/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Summary: Ensure typed array validation is called for TypedArray.prototype.subarray.

const otherGlobal = typeof newGlobal === "function" ? newGlobal() : undefined;
const typedArrayLengths = [0, 1, 1024];

function createTestCases(TAConstructor, constructor, constructorCrossRealm) {
    let testCases = [];
    testCases.push({
        species: constructor,
        method: TAConstructor.prototype.subarray,
        error: TypeError,
    });
    if (otherGlobal) {
        testCases.push({
            species: constructorCrossRealm,
            method: TAConstructor.prototype.subarray,
            error: TypeError,
        });
        testCases.push({
            species: constructor,
            method: otherGlobal[TAConstructor.name].prototype.subarray,
            // Note: subarray uses CallTypedArrayMethodIfWrapped, which results
            //       in throwing a TypeError from the wrong Realm.
            error: TypeError,
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
            assertThrowsInstanceOf(() => method.call(ta, 0), error);
            assertEq(callCount, ++expectedCallCount);
        }
    }

    for (let {species, method, error} of testCases) {
        for (let length of typedArrayLengths) {
            let ta = new TAConstructor(length);
            ta.constructor = {[Symbol.species]: species};
            assertThrowsInstanceOf(() => method.call(ta, 0, 0), error);
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
            // Note: TypedArray |a| is (currently) created in this global, not
            //       |otherGlobal|, because a typed array and its buffer must
            //       use the same compartment.
            detachArrayBuffer(a.buffer);
            callCount += 1;
            return a;
        }
        let testCases = createTestCases(TAConstructor, DetachConstructor, DetachConstructorCrossRealm);

        for (let {species, method, error} of testCases) {
            for (let length of typedArrayLengths) {
                let ta = new TAConstructor(length);
                ta.constructor = {[Symbol.species]: species};
                assertThrowsInstanceOf(() => method.call(ta, 0), error);
                assertEq(callCount, ++expectedCallCount);
            }
        }

        for (let {species, method, error} of testCases) {
            for (let length of typedArrayLengths) {
                let ta = new TAConstructor(length);
                ta.constructor = {[Symbol.species]: species};
                assertThrowsInstanceOf(() => method.call(ta, 0, 0), error);
                assertEq(callCount, ++expectedCallCount);
            }
        }
    }
}

if (typeof reportCompare === "function")
    reportCompare(0, 0);
