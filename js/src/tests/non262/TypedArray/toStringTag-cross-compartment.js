const TypedArrayPrototype = Object.getPrototypeOf(Int8Array.prototype);
const {get: toStringTag} = Object.getOwnPropertyDescriptor(TypedArrayPrototype, Symbol.toStringTag);

const otherGlobal = newGlobal();

for (let constructor of anyTypedArrayConstructors) {
    let ta = new otherGlobal[constructor.name](0);
    assertEq(toStringTag.call(ta), constructor.name);
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
