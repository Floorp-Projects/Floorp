function testNonEnumerableVisited() {
    // Non-enumerable properties must still be included in the duplicate-property
    // checking.

    var proto1 = Object.create(null);
    Object.defineProperty(proto1, "x", {enumerable: true, value: 1});

    var proto2 = Object.create(proto1);
    Object.defineProperty(proto2, "x", {enumerable: false, value: 1});

    for (var p in proto2) {
        throw "Shouldn't enumerate any properties";
    }

    var o = Object.create(proto2);
    for (var p in o) {
        throw "Shouldn't enumerate any properties";
    }
}
testNonEnumerableVisited();

function testEnumerableOnProto() {
    var iter = o => {
        var props = [];
        for (var p in o) { props.push(p); }
        return props;
    };

    // Test dense elements on the proto chain are included.
    var props = iter(Object.create({0: 1}));
    assertEq(JSON.stringify(props), '["0"]');

    // Test typed array elements on the proto chain are included.
    props = iter(Object.create(new Int32Array(2)));
    assertEq(JSON.stringify(props), '["0","1"]');

    // Test sparse elements on the proto chain are included.
    props = iter(Object.create({1234567: 1}));
    assertEq(JSON.stringify(props), '["1234567"]');
}
testEnumerableOnProto();
