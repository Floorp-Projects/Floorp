const prototypes = [
    Map.prototype,
    Set.prototype,
    WeakMap.prototype,
    WeakSet.prototype,
    Date.prototype,
];

for (const prototype of prototypes) {
    delete prototype[Symbol.toStringTag];
    assertEq(Object.prototype.toString.call(prototype), "[object Object]");
}
