var max = 40;
setJitCompilerOption("ion.warmup.trigger", max - 10);

function simple() {
    var array = [{a: 1}, {b: 1, a: 1}, {c: 1, a: 1}];
    for (var i = 0; i < array.length; i++) {
        var x = array[i];
        assertEq(x.hasOwnProperty("a"), true);
        assertEq(x.hasOwnProperty("d"), false);
    }
}

function megamorphic() {
    var array = [{a: 1}, {b: 1, a: 1}, {c: 1, a: 1},
                 {a: 1, b: 1}, {c: 1, e: 1, a: 1},
                 {e: 1, f: 1, a: 1, g: 1},
                 {e: 1, f: 1, a: 1, g: 1, h: 1}];
    for (var i = 0; i < array.length; i++) {
        var x = array[i];
        assertEq(x.hasOwnProperty("a"), true);
        assertEq(x.hasOwnProperty("d"), false);
    }
}

function key() {
    var sym = Symbol(), sym2 = Symbol();
    var keys = [[sym, true], [sym2, false],
                ["a", true], ["b", false],
                [{}, false]];
    var obj = {[sym]: 1, a: 1};
    for (var i = 0; i < keys.length; i++) {
        var [key, result] = keys[i];
        assertEq(obj.hasOwnProperty(key), result);
    }
}

function test() {
    for (var i = 0; i < max; i++) {
        simple();
        megamorphic();
        key();
    }
}

test();
test();
