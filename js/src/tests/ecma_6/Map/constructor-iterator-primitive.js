var BUGNUMBER = 1021835;
var summary = "Returning non-object from @@iterator should throw";

print(BUGNUMBER + ": " + summary);

let ctors = [
    Map,
    Set,
    WeakMap,
    WeakSet
];

let primitives = [
    1,
    true,
    undefined,
    null,
    "foo",
    Symbol.iterator
];

for (let ctor of ctors) {
    for (let primitive of primitives) {
        let arg = {
            [Symbol.iterator]() {
                return primitive;
            }
        };
        assertThrowsInstanceOf(() => new ctor(arg), TypeError);
    }
}

if (typeof reportCompare === "function")
  reportCompare(0, 0);
