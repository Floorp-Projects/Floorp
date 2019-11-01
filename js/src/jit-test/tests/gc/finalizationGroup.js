// |jit-test| --enable-weak-refs

function checkPropertyDescriptor(obj, property, writable, enumerable,
                                 configurable) {
    let desc = Object.getOwnPropertyDescriptor(obj, property);
    assertEq(typeof desc, "object");
    assertEq(desc.writable, writable);
    assertEq(desc.enumerable, enumerable);
    assertEq(desc.configurable, configurable);
}

function assertThrowsTypeError(thunk) {
    let error;
    try {
        thunk();
    } catch (e) {
        error = e;
    }
    assertEq(error instanceof TypeError, true);
}

// 3.1 The FinalizationGroup Constructor
assertEq(typeof this.FinalizationGroup, "function");

// 3.2 Properties of the FinalizationGroup Constructor
assertEq(Object.getPrototypeOf(FinalizationGroup), Function.prototype);

// 3.2.1 FinalizationGroup.prototype
checkPropertyDescriptor(FinalizationGroup, 'prototype', false, false, false);

// 3.3 Properties of the FinalizationGroup Prototype Object
let proto = FinalizationGroup.prototype;
assertEq(Object.getPrototypeOf(proto), Object.prototype);

// 3.3.1 FinalizationGroup.prototype.constructor
assertEq(proto.constructor, FinalizationGroup);

//3.3.5 FinalizationGroup.prototype [ @@toStringTag ]
assertEq(proto[Symbol.toStringTag], "FinalizationGroup");
checkPropertyDescriptor(proto, Symbol.toStringTag, false, false, true);

assertThrowsTypeError(() => new FinalizationGroup());
assertThrowsTypeError(() => new FinalizationGroup(1));
let group = new FinalizationGroup(x => 1);
