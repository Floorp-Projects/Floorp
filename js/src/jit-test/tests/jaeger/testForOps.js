// vim: set ts=4 sw=4 tw=99 et:

function assertObjectsEqual(obj1, obj2) {
    assertEq(obj1.a, obj2.a);
    assertEq(obj1.b, obj2.b);
    assertEq(obj1.c, obj2.c);
    assertEq(obj1.d, obj2.d);
    assertEq(obj2.a, 1);
    assertEq(obj2.b, "bee");
    assertEq(obj2.c, "crab");
    assertEq(obj2.d, 12);
}

function forName(obj) {
    assertJit();
    eval('');
    var r = { };
    for (x in obj)
        r[x] = obj[x];
    return r;
}

function forProp(obj) {
    assertJit();
    var r = { };
    var c = { };
    for (c.x in obj)
        r[c.x] = obj[c.x];
    return r;
}

function forElem(obj, x) {
    assertJit();
    var r = { };
    var c = { };
    for (c[x] in obj)
        r[c[x]] = obj[c[x]];
    return r;
}

function forLocal(obj) {
    assertJit();
    var r = { };
    for (var x in obj)
        r[x] = obj[x];
    return r;
}

function forArg(obj, x) {
    assertJit();
    var r = { };
    for (x in obj)
        r[x] = obj[x];
    return r;
}

var obj = { a: 1, b: "bee", c: "crab", d: 12 };
assertObjectsEqual(obj, forName(obj));
assertObjectsEqual(obj, forProp(obj));
assertObjectsEqual(obj, forElem(obj, "v"));
assertObjectsEqual(obj, forLocal(obj));
assertObjectsEqual(obj, forArg(obj));

