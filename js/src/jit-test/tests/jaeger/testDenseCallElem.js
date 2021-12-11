// vim: set ts=8 sts=4 et sw=4 tw=99:

function fillDense(a) {
}

function testDenseUKeyUArray(a, key) {
    a.push(function () { return this[3]; });
    a.push(function () { return this[4]; });
    a.push(function() { return this[5]; });
    a.push(20);
    a.push("hi");
    a.push(500);
    assertEq(a[key](), 20);
    assertEq(a[key + 1](), "hi");
    assertEq(a[key + 2](), 500);
}

function testDenseVKeyUArray(a) {
    a.push(function () { return this[3]; });
    a.push(function () { return this[4]; });
    a.push(function() { return this[5]; });
    a.push(20);
    a.push("hi");
    a.push(500);
    var key = a.length & 1;
    assertEq(a[key](), 20);
    assertEq(a[(key + 1) & 3](), "hi");
    assertEq(a[(key + 2) & 3](), 500);
}

function testDenseKKeyUArray(a, key) {
    a.push(function () { return this[3]; });
    a.push(function () { return this[4]; });
    a.push(function() { return this[5]; });
    a.push(20);
    a.push("hi");
    a.push(500);
    assertEq(a[0](), 20);
    assertEq(a[1](), "hi");
    assertEq(a[2](), 500);
}

function testDenseUKeyVArray(key) {
    var a = [function () { return this[3]; },
             function () { return this[4]; },
             function() { return this[5]; },
             20,
             "hi",
             500];
    assertEq(a[key](), 20);
    assertEq(a[key + 1](), "hi");
    assertEq(a[key + 2](), 500);
}

function testDenseVKeyVArray() {
    var a = [function () { return this[3]; },
             function () { return this[4]; },
             function() { return this[5]; },
             20,
             "hi",
             500];
    var key = a.length & 1;
    assertEq(a[key](), 20);
    assertEq(a[(key + 1) & 3](), "hi");
    assertEq(a[(key + 2) & 3](), 500);
}

function testDenseKKeyVArray() {
    var a = [function () { return this[3]; },
             function () { return this[4]; },
             function() { return this[5]; },
             20,
             "hi",
             500];
    assertEq(a[0](), 20);
    assertEq(a[1](), "hi");
    assertEq(a[2](), 500);
}

for (var i = 0; i < 5; i++) {
    testDenseUKeyUArray([], 0);
    testDenseVKeyUArray([]);
    testDenseKKeyUArray([]);
    testDenseUKeyVArray(0);
    testDenseVKeyVArray();
    testDenseKKeyVArray();
}


