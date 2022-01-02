// |reftest| skip-if(!xulRuntime.shell) -- needs drainJobQueue

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
    let arg = {
        [Symbol.iterator]() {
            return primitive;
        }
    };
    assertEventuallyThrows(Promise.all(arg), TypeError);
    assertEventuallyThrows(Promise.race(arg), TypeError);
}

if (typeof reportCompare === "function")
  reportCompare(0, 0);
