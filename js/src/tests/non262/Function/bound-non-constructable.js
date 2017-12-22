var objects = [
    Math.sin.bind(null),
    new Proxy(Math.sin.bind(null), {}),
    Function.prototype.bind.call(new Proxy(Math.sin, {}))
]

for (var obj of objects) {
    // Target is not constructable, so a new array should be created internally.
    assertDeepEq(Array.from.call(obj, [1, 2, 3]), [1, 2, 3]);
    assertDeepEq(Array.of.call(obj, 1, 2, 3), [1, 2, 3]);

    // Make sure they are callable, but not constructable.
    obj();
    assertThrowsInstanceOf(() => new obj, TypeError);
}

reportCompare(0, 0, 'ok');
