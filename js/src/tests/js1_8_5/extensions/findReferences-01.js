// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/
// Contributor: Jim Blandy

if (typeof findReferences == "function") {
    function C() {}
    var o = new C;
    o.x = {};               // via ordinary property
    o[42] = {};             // via numeric property
    o[123456789] = {};      // via ridiculous numeric property
    o.myself = o;           // self-references should be reported
    o.alsoMyself = o;       // multiple self-references should all be reported

    assertEq(referencesVia(o, 'group; group_proto', C.prototype), true);
    assertEq(referencesVia(o, 'shape; base; parent', this), true);
    assertEq(referencesVia(o, 'x', o.x), true);
    assertEq(referencesVia(o, 'objectElements[42]', o[42]), true);
    assertEq(referencesVia(o, '123456789', o[123456789]), true);
    assertEq(referencesVia(o, 'myself', o), true);
    assertEq(referencesVia(o, 'alsoMyself', o), true);

    function g() { return 42; }
    function s(v) { }
    var p = Object.defineProperty({}, 'a', { get:g, set:s });
    assertEq(referencesVia(p, 'shape; getter', g), true);
    assertEq(referencesVia(p, 'shape; setter', s), true);

    // If there are multiple objects with the same shape referring to a getter
    // or setter, findReferences should get all of them, even though the shape
    // gets 'marked' the first time we visit it.
    var q = Object.defineProperty({}, 'a', { get:g, set:s });
    assertEq(referencesVia(p, 'shape; getter', g), true);
    assertEq(referencesVia(q, 'shape; getter', g), true);

    // If we extend each object's shape chain, both should still be able to
    // reach the getter, even though the two shapes are each traversed twice.
    p.b = 9;
    q.b = 9;
    assertEq(referencesVia(p, 'shape; parent; getter', g), true);
    assertEq(referencesVia(q, 'shape; parent; getter', g), true);

    // These are really just ordinary own property references.
    assertEq(referencesVia(C, 'prototype', Object.getPrototypeOf(o)), true);
    assertEq(referencesVia(Object.getPrototypeOf(o), 'constructor', C), true);

    // Dense arrays should work, too.
    a = [];
    a[1] = o;
    assertEq(referencesVia(a, 'objectElements[1]', o), true);

    reportCompare(true, true);
} else {
    reportCompare(true, true, "test skipped: findReferences is not a function");
}
