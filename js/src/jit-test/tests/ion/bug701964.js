load(libdir + 'array-compare.js');

function f(v) {
    return v.length;
}

function g(v) {
    return v.length;
}

function h(v) {
    return v.length;
}

function aliasCheck(v) {
    v[0] = v.length;
    v[1] = v.length;
    v[2] = v.length;
    return v;
}

for(let i = 41; i; i--) {
    assertEq(f([]), 0);
    assertEq(f([0]), 1);
    assertEq(f([0, 1]), 2);
    assertEq(f([0, 1, 2]), 3);
    assertEq(g(""), 0);
    assertEq(g("1"), 1);
    assertEq(g("12"), 2);
    assertEq(g("123"), 3);
    assertEq(h({}), undefined);
    assertEq(h({a: 1}), undefined);
    assertEq(h({a: 1, b: 2}), undefined);
    assertEq(h({a: 1, b: 2, length: 3}), 3);
    assertEq(arraysEqual(aliasCheck([0, 1, 2]), [3, 3, 3]), true);
}
