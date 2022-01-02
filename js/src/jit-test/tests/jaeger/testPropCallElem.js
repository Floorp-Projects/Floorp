// vim: set ts=8 sts=4 et sw=4 tw=99:

function testUKeyUObject(a, key1, key2, key3) {
    a.a = function () { return this.d; }
    a.b = function () { return this.e; }
    a.c = function() { return this.f; }
    a.d = 20;
    a.e = "hi";
    a.f = 500;
    assertEq(a[key1](), 20);
    assertEq(a[key2](), "hi");
    assertEq(a[key3](), 500);
}

function testVKeyUObject(a, key1, key2, key3) {
    a.a = function () { return this.d; }
    a.b = function () { return this.e; }
    a.c = function() { return this.f; }
    a.d = 20;
    a.e = "hi";
    a.f = 500;
    assertEq(a["" + key1](), 20);
    assertEq(a["" + key2](), "hi");
    assertEq(a["" + key3](), 500);
}

function testKKeyUObject(a) {
    a.a = function () { return this.d; }
    a.b = function () { return this.e; }
    a.c = function() { return this.f; }
    a.d = 20;
    a.e = "hi";
    a.f = 500;
    var key1 = "a";
    var key2 = "b";
    var key3 = "c";
    assertEq(a[key1](), 20);
    assertEq(a[key2](), "hi");
    assertEq(a[key3](), 500);
}

function testUKeyVObject(key1, key2, key3) {
    a = { a: function () { return this.d; },
          b: function () { return this.e; },
          c: function () { return this.f; },
          d: 20,
          e: "hi",
          f: 500
    };
    assertEq(a[key1](), 20);
    assertEq(a[key2](), "hi");
    assertEq(a[key3](), 500);
}

function testVKeyVObject(key1, key2, key3) {
    a = { a: function () { return this.d; },
          b: function () { return this.e; },
          c: function () { return this.f; },
          d: 20,
          e: "hi",
          f: 500
    };
    assertEq(a["" + key1](), 20);
    assertEq(a["" + key2](), "hi");
    assertEq(a["" + key3](), 500);
}

function testKKeyVObject(a) {
    a = { a: function () { return this.d; },
          b: function () { return this.e; },
          c: function () { return this.f; },
          d: 20,
          e: "hi",
          f: 500
    };
    var key1 = "a";
    var key2 = "b";
    var key3 = "c";
    assertEq(a[key1](), 20);
    assertEq(a[key2](), "hi");
    assertEq(a[key3](), 500);
}

for (var i = 0; i < 5; i++) {
    testUKeyUObject({}, "a", "b", "c");
    testVKeyUObject({}, "a", "b", "c");
    testKKeyUObject({});
    testUKeyVObject("a", "b", "c");
    testVKeyVObject("a", "b", "c");
    testKKeyVObject();
}


