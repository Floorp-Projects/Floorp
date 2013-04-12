// vim: set ts=4 sw=4 tw=99 et:

function testUKeyUObject(a, key1, key2, key3) {
    a.a = function () { return this.d; }
    a.b = function () { return this.e; }
    a.c = function() { return this.f; }
    a.d = 20;
    a.e = "hi";
    a.f = 500;
    delete a["b"];
    Object.defineProperty(a, "b", { get: function () { return function () { return this.e; } } });
    assertEq(a[key1](), 20);
    assertEq(a[key2](), "hi");
    assertEq(a[key3](), 500);
}

for (var i = 0; i < 5; i++)
    testUKeyUObject({}, "a", "b", "c");


