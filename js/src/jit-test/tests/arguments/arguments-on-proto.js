function create() {
    return Object.create(arguments, {2: {value: "shadowed"}});
}

function createStrict() {
    "use strict";
    return Object.create(arguments, {40: {value: "shadowed2"}});
}

function f() {
    var args = [createStrict(10, 20, 30, 40), create(1, 2, 3)];

    var threshold = getJitCompilerOptions()["ion.warmup.trigger"] + 101;

    for (var i = 0; i < threshold; i++) {
        // We switch between different arguments objects, to make
        // sure the right IC is triggered.
        var a = args[i % 2];
        assertEq(a.length, (i % 2) ? 3 : 4);
        assertEq(a[0], (i % 2) ? 1 : 10);
        assertEq(a[1], (i % 2) ? 2 : 20);
        assertEq(a[2], (i % 2) ? "shadowed" : 30);
        assertEq(a[3], (i % 2) ? undefined : 40);
    }
}

f();
