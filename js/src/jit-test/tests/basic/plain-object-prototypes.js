const prototypes = [
    Map.prototype,
    Set.prototype,
    WeakMap.prototype,
    WeakSet.prototype,
    Date.prototype,
    Error.prototype,
    InternalError.prototype,
    EvalError.prototype,
    RangeError.prototype,
    ReferenceError.prototype,
    SyntaxError.prototype,
    TypeError.prototype,
    URIError.prototype,
];

for (const prototype of prototypes) {
    delete prototype[Symbol.toStringTag];
    assertEq(Object.prototype.toString.call(prototype), "[object Object]");
}
