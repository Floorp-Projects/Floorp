var BUGNUMBER = 1021835;
var summary = "Returning non-object from @@iterator should throw";

print(BUGNUMBER + ": " + summary);

let primitives = [
    1,
    true,
    undefined,
    null,
    "foo",
    Symbol.iterator
];

for (let primitive of primitives) {
    let obj = {
        [Symbol.iterator]() {
            return primitive;
        }
    };
    assertThrowsInstanceOf(() => {
        for (let x of obj) {
        }
    }, TypeError);
}

if (typeof reportCompare === "function")
  reportCompare(0, 0);
