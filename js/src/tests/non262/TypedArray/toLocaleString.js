const TypedArrayPrototype = Object.getPrototypeOf(Int8Array.prototype);

// %TypedArrayPrototype% has an own "toLocaleString" function property.
assertEq(TypedArrayPrototype.hasOwnProperty("toLocaleString"), true);
assertEq(typeof TypedArrayPrototype.toLocaleString, "function");

// The initial value of %TypedArrayPrototype%.toLocaleString is not Array.prototype.toLocaleString.
assertEq(TypedArrayPrototype.toLocaleString === Array.prototype.toLocaleString, false);

// The concrete TypedArray prototypes do not have an own "toLocaleString" property.
assertEq(anyTypedArrayConstructors.every(c => !c.hasOwnProperty("toLocaleString")), true);

assertDeepEq(Object.getOwnPropertyDescriptor(TypedArrayPrototype, "toLocaleString"), {
    value: TypedArrayPrototype.toLocaleString,
    writable: true,
    enumerable: false,
    configurable: true,
});

assertEq(TypedArrayPrototype.toLocaleString.name, "toLocaleString");
assertEq(TypedArrayPrototype.toLocaleString.length, 0);

// It's not a generic method.
assertThrowsInstanceOf(() => TypedArrayPrototype.toLocaleString.call(), TypeError);
for (let invalid of [void 0, null, {}, [], function(){}, true, 0, "", Symbol()]) {
    assertThrowsInstanceOf(() => TypedArrayPrototype.toLocaleString.call(invalid), TypeError);
}

const localeOne = 1..toLocaleString(),
      localeTwo = 2..toLocaleString(),
      localeSep = [,,].toLocaleString();

for (let constructor of anyTypedArrayConstructors) {
    assertEq(new constructor([]).toLocaleString(), "");
    assertEq(new constructor([1]).toLocaleString(), localeOne);
    assertEq(new constructor([1, 2]).toLocaleString(), localeOne + localeSep + localeTwo);
}

const originalNumberToLocaleString = Number.prototype.toLocaleString;

// Calls Number.prototype.toLocaleString on each element.
for (let constructor of anyTypedArrayConstructors) {
    Number.prototype.toLocaleString = function() {
        "use strict";

        // Ensure this-value is not boxed.
        assertEq(typeof this, "number");

        // Test ToString is applied.
        return {
            valueOf: () => {
                throw new Error("valueOf called");
            },
            toString: () => {
                return this + 10;
            }
        };
    };
    let typedArray = new constructor([1, 2]);
    assertEq(typedArray.toLocaleString(), "11" + localeSep + "12");
}
Number.prototype.toLocaleString = originalNumberToLocaleString;

// Calls Number.prototype.toLocaleString from the current Realm.
const otherGlobal = newGlobal();
for (let constructor of anyTypedArrayConstructors) {
    Number.prototype.toLocaleString = function() {
        "use strict";
        called = true;
        return this;
    };
    let typedArray = new otherGlobal[constructor.name]([1]);
    let called = false;
    assertEq(TypedArrayPrototype.toLocaleString.call(typedArray), "1");
    assertEq(called, true);
}
Number.prototype.toLocaleString = originalNumberToLocaleString;

if (typeof reportCompare === "function")
    reportCompare(true, true);
