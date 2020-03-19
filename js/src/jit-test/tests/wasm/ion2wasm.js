var ITERATIONS = 10;
var INNER_ITERATIONS = 100;

let instance = wasmEvalText(`(module
    (func (export "add") (result i32) (param i32) (param i32)
     local.get 0
     local.get 1
     i32.add
    )

    (func (export "no_arg") (result i32)
     i32.const 42
     i32.const 58
     i32.add
    )

    (global $g (mut i32) (i32.const 0))

    (func (export "set_global_one") (param i32)
     local.get 0
     global.set $g
    )

    (func (export "set_global_two") (param i32) (param i32)
     local.get 0
     local.get 1
     i32.add
     global.set $g
    )

    (func (export "glob") (result i32)
     global.get $g
    )
)`).exports;

function run(name, func) {
    for (let i = ITERATIONS; i --> 0;) {
        func();
    }
}

function testCallKnown() {
    for (let i = 0; i < INNER_ITERATIONS; i++) {
        assertEq(instance.add(i, i + 1), 2*i + 1);
    }
}

function testCallKnownRectifying() {
    for (let i = 0; i < INNER_ITERATIONS; i++) {
        assertEq(instance.add(i + 1), i+1);
    }
}

function jsAdd(x, y) {
    return (x|0) + (y|0) | 0;
}

function testCallGeneric() {
    var arr = [instance.add, jsAdd];
    for (let i = 0; i < INNER_ITERATIONS; i++) {
        assertEq(arr[i%2](i, i+1), 2*i + 1);
    }
}

function testCallGenericRectifying() {
    var arr = [instance.add, jsAdd];
    for (let i = 0; i < INNER_ITERATIONS; i++) {
        assertEq(arr[i%2](i+1), i + 1);
    }
}

function testCallScriptedGetter() {
    var obj = {};
    Object.defineProperty(obj, 'x', {
        get: instance.no_arg
    });
    for (let i = 0; i < INNER_ITERATIONS; i++) {
        assertEq(obj.x, 100);
    }
}

function testCallScriptedGetterRectifying() {
    var obj = {};
    Object.defineProperty(obj, 'x', {
        // Missing two arguments.
        get: instance.add
    });
    for (let i = 0; i < INNER_ITERATIONS; i++) {
        assertEq(obj.x, 0);
    }
}

function testCallScriptedSetter() {
    var obj = {};
    Object.defineProperty(obj, 'x', {
        set: instance.set_global_one
    });
    for (let i = 0; i < INNER_ITERATIONS; i++) {
        obj.x = i;
    }
    assertEq(instance.glob(), INNER_ITERATIONS-1);
}

function testCallScriptedSetterRectifying() {
    var obj = {};
    Object.defineProperty(obj, 'x', {
        set: instance.set_global_two
    });
    for (let i = 0; i < INNER_ITERATIONS; i++) {
        obj.x = i;
    }
    assertEq(instance.glob(), INNER_ITERATIONS-1);
}

function testFunctionApplyArray() {
    for (let i = 0; i < INNER_ITERATIONS; i++) {
        assertEq(instance.add.apply(null, [i, i + 1]), 2*i+1);
    }
}

function testFunctionApplyArrayRectifying() {
    for (let i = 0; i < INNER_ITERATIONS; i++) {
        assertEq(instance.add.apply(null, [i + 1]), i+1);
    }
}

function testFunctionApplyArgs() {
    function wrapper() {
        assertEq(instance.add.apply(null, arguments), 2*arguments[0]+1);
    }
    for (let i = 0; i < INNER_ITERATIONS; i++) {
        wrapper(i, i + 1);
    }
}

function testFunctionApplyArgsRectifying() {
    function wrapper() {
        assertEq(instance.add.apply(null, arguments), arguments[0]);
    }
    for (let i = 0; i < INNER_ITERATIONS; i++) {
        wrapper(i + 1);
    }
}

function testFunctionCall() {
    for (let i = 0; i < INNER_ITERATIONS; i++) {
        assertEq(instance.add.call(null, i, i + 1), 2*i+1);
    }
}

function testFunctionCallRectifying() {
    for (let i = 0; i < INNER_ITERATIONS; i++) {
        assertEq(instance.add.call(null, i + 1), i+1);
    }
}

run('call known', testCallKnown);
run('call known rectifying', testCallKnownRectifying);

run('call generic', testCallGeneric);
run('call generic rectifying', testCallGenericRectifying);

run('scripted getter', testCallScriptedGetter);
run('scripted getter rectifiying', testCallScriptedGetterRectifying);
run('scripted setter', testCallScriptedSetter);
run('scripted setter rectifiying', testCallScriptedSetterRectifying);

run('function.apply array', testFunctionApplyArray);
run('function.apply array rectifying', testFunctionApplyArrayRectifying);
run('function.apply args', testFunctionApplyArgs);
run('function.apply args rectifying', testFunctionApplyArgsRectifying);

run('function.call', testFunctionCall);
run('function.call rectifying', testFunctionCallRectifying);
