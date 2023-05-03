var max = 40;
setJitCompilerOption("ion.warmup.trigger", max - 10);

function simple() {
    var array = [{a: 1}, {b: 1, a: 1}, {c: 1, a: 1}];
    for (var i = 0; i < array.length; i++) {
        var x = array[i];
        assertEq("a" in x, true);
        assertEq("d" in x, false);
    }
}

function megamorphic() {
    var array = [{a: 1}, {b: 1, a: 1}, {c: 1, a: 1},
                 {a: 1, b: 1}, {c: 1, e: 1, a: 1},
                 {__proto__:{e: 1, f: 1, a: 1, g: 1}},
                 {__proto__:{e: 1, f: 1, a: 1, g: 1, h: 1}}];
    for (var i = 0; i < array.length; i++) {
        var x = array[i];
        assertEq("a" in x, true);
        assertEq("d" in x, false);
    }
}

function proto() {
    var base = {a: 1};
    var array = [{__proto__: base},
                 {__proto__: base, b: 1, a: 1},
                 {__proto__: base, c: 1, a: 1}];
    for (var j = 0; j < 2; j++) {
        for (var i = 0; i < array.length; i++) {
            var x = array[i];
            assertEq("a" in x, true);
            assertEq("d" in x, (j > 0));
        }
        base.d = 1; // Define property on prototype
    }
}

function test() {
    for (var i = 0; i < max; i++) {
        simple();
        megamorphic();
        proto();
    }
}

test();
test();
