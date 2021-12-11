// Basics of @@unscopables support.

// In with(obj), if obj[@@unscopables][id] is truthy, then the identifier id
// is not present as a binding in the with-block's scope.
var x = "global";
with ({x: "with", [Symbol.unscopables]: {x: true}})
    assertEq(x, "global");

// But if obj[@@unscopables][id] is false or not present, there is a binding.
with ({y: "with", z: "with", [Symbol.unscopables]: {y: false}}) {
    assertEq(y, "with");
    assertEq(z, "with");
}

// ToBoolean(obj[@@unscopables][id]) determines whether there's a binding.
let someValues = [0, -0, NaN, "", undefined, null, "x", {}, []];
for (let v of someValues) {
    with ({x: "with", [Symbol.unscopables]: {x: v}})
        assertEq(x, v ? "global" : "with");
}

reportCompare(0, 0);
