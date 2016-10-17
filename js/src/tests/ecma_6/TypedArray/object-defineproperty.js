var obj = new Int32Array(2);
obj[0] = 100;

var throws = [
    // Disallow accessors
    {get: undefined},
    {set: undefined},
    {get: undefined, set: undefined},
    {get: function() {}},
    {set: function() {}},
    {get: function() {}, set: function() {}},

    {configurable: true},
    {enumerable: false},
    {writable: false},

    {configurable: true, writable: true},
    {enumerable: false, configurable: false},

    {configurable: true, value: 15}
];

for (var desc of throws) {
    assertThrowsInstanceOf(function() { Object.defineProperty(obj, 0, desc); }, TypeError);
    assertThrowsInstanceOf(function() { Object.defineProperties(obj, {0: desc}); }, TypeError);
}

Object.defineProperty(obj, 0, {});
Object.defineProperty(obj, 0, {configurable: false});
Object.defineProperty(obj, 0, {enumerable: true});
Object.defineProperty(obj, 0, {writable: true});

assertEq(obj[0], 100);

Object.defineProperty(obj, 0, {configurable: false, value: 15});
assertEq(obj[0], 15);
Object.defineProperty(obj, 0, {enumerable: true, value: 16});
assertEq(obj[0], 16);
Object.defineProperty(obj, 0, {writable: true, value: 17});
assertEq(obj[0], 17);
Object.defineProperty(obj, 0, {value: 18});
assertEq(obj[0], 18);

var desc = Object.getOwnPropertyDescriptor(obj, 0);
assertEq(desc.configurable, false);
assertEq(desc.enumerable, true);
assertEq(desc.writable, true);
assertEq(desc.value, 18);
assertEq('get' in desc, false);
assertEq('set' in desc, false);

Object.defineProperties(obj, {0: {value: 20}, 1: {value: 42}});
assertEq(obj[0], 20);
assertEq(obj[1], 42);

anyTypedArrayConstructors.forEach(constructor => {
    var obj = new constructor(4);
    obj[0] = 100;
    obj[1] = 200;

    for (var v of [20, 300, -10, Math.pow(2, 32), -Math.pow(2, 32), NaN]) {
        Object.defineProperty(obj, 0, {value: v});
        obj[1] = v;
        assertEq(obj[0], obj[1]);
    }
});

reportCompare(true, true, "test");
