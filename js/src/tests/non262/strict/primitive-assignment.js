let primitives = [
    10,
    false,
    "test",
    Symbol()
];

let key = "key";

for (let value of primitives) {
    // Doesn't throw outside strict mode.
    assertEq(value.x = 5, 5);
    assertEq(value[key] = 6, 6);

    assertThrowsInstanceOf(function() { "use strict"; value.x = 5; }, TypeError);
    assertThrowsInstanceOf(function() { "use strict"; value[key] = 6; }, TypeError);

    let target = {};
    assertEq(Reflect.set(target, key, 5, value), false);
    assertEq(key in target, false);
}

reportCompare(true, true);
