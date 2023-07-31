// |reftest| 

function test(otherGlobal) {
    assertEq(TypeError !== otherGlobal.TypeError, true);
    assertEq(Object.getPrototypeOf(TypeError) !== Object.getPrototypeOf(otherGlobal.TypeError), true);
    assertEq(RangeError !== otherGlobal.RangeError, true);
    assertEq(Object.getPrototypeOf(RangeError) !== Object.getPrototypeOf(otherGlobal.RangeError), true);

    
    var arrayLike = {
        get "0"() {
            throw new Error("Get 0");
        },
        get "4294967295" () { // 2 ** 32 - 1
            throw new Error("Get 2147483648");
        },
        get "4294967296" () { // 2 ** 32
            throw new Error("Get 2147483648");
        },
        length: 2 ** 32
    };

    let gToSorted = otherGlobal.Array.prototype.toSorted;
    let gToSpliced = otherGlobal.Array.prototype.toSpliced;
    let gToReversed = otherGlobal.Array.prototype.toReversed;
    let gWith = otherGlobal.Array.prototype.with;

    let typeErrorCalls = [
        ["toSorted - bad comparator", () => gToSorted.call([], 5)],
        ["toSorted - this is null", () => gToSorted.call(null)],
        ["toSpliced - array too long", () => {
            var oldLen = arrayLike.length;
            arrayLike.length = 2**53 - 1;
            gToSpliced.call(arrayLike, 0, 0, 1);
            arrayLike.length = oldLen;
        }]
    ]

    let rangeErrorCalls = [
        ["toSorted - array too long", () => gToSorted.call(arrayLike)],
        ["toReversed - array too long", () => gToReversed.call(arrayLike)],
        ["toSpliced - adding elements would exceed max array length", () => gToSpliced.call(arrayLike, 0, 0)],
        ["with - index out of range", () => gWith.call([0, 1, 2], 3, 7)],
        ["with - negative index", () => gWith.call([0, 1, 2], -4, 7)],
        ["with - array too long", () => gWith.call(arrayLike, 0, 0)]
    ]

    // For each erroneous case, make sure the error comes from
    // the other realm (not this realm)
    for (const [message, f] of typeErrorCalls) {
        try {
            f();
        } catch (exc) {
            assertEq(exc instanceof TypeError, false, message + " threw TypeError from wrong realm");
            assertEq(exc instanceof otherGlobal.TypeError, true, message + " didn't throw TypeError from other realm");
            assertEq(Object.getPrototypeOf(exc) !== Object.getPrototypeOf(TypeError), true,
                     message + " TypeError has wrong prototype");
        }
    }

    for (const [message, f] of rangeErrorCalls) {
        try {
            f();
        } catch (exc) {
            assertEq(exc instanceof RangeError, false, message + " threw RangeError from wrong realm");
            assertEq(exc instanceof otherGlobal.RangeError, true, message + " didn't throw RangeError from other realm");
            assertEq(Object.getPrototypeOf(exc) !== Object.getPrototypeOf(RangeError), true,
                     message + " TypeError has wrong prototype");
        }
    }
}

test(newGlobal());
test(newGlobal({newCompartment: true}));

if (typeof reportCompare === "function")
  reportCompare(0, 0);
