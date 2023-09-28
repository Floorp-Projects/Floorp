// The cache lookup must happen after ensuring there are no dense elements.
function testNewDenseElement() {
    var from = {x: 1, y: 2, z: 3};

    for (var i = 0; i < 10; i++) {
        if (i === 6) {
            from[0] = 1;
        }
        var to = Object.assign({}, from);
        if (i >= 6) {
            assertEq(JSON.stringify(to), '{"0":1,"x":1,"y":2,"z":3}');
        } else {
            assertEq(JSON.stringify(to), '{"x":1,"y":2,"z":3}');
        }
    }
}
testNewDenseElement();

// The cache lookup must happen after ensuring there are non-writable
// properties on the proto chain.
function testProtoNonWritable() {
    var proto = {x: 1};
    var from = {x: 1, y: 2, z: 3};

    for (var i = 0; i < 10; i++) {
        if (i === 6) {
            Object.freeze(proto);
        }

        var to = Object.create(proto);
        var ex = null;
        try {
            Object.assign(to, from);
        } catch (e) {
            ex = e;
        }

        assertEq(ex instanceof TypeError, i > 5);

        if (i <= 5) {
            assertEq(JSON.stringify(to), '{"x":1,"y":2,"z":3}');
        } else {
            assertEq(JSON.stringify(to), '{}');
        }
    }
}
testProtoNonWritable();

function testDictionary1() {
    var from = {a: 1, b: 2, c: 3};
    delete from.a;
    for (var i = 0; i < 10; i++) {
        var to = Object.assign({}, from);
        assertEq(JSON.stringify(to), '{"b":2,"c":3}');
    }
}
testDictionary1();

function testDictionary2() {
    var from = {a: 1, b: 2, c: 3};
    delete from.a;
    from.a = 4;
    for (var i = 0; i < 10; i++) {
        var to = Object.assign({}, from);
        assertEq(JSON.stringify(to), '{"b":2,"c":3,"a":4}');
    }
}
testDictionary2();
