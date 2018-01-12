try {
    enableSingleStepProfiling();
    disableSingleStepProfiling();
} catch(e) {
    // Single step profiling not supported here.
    quit();
}

const Module = WebAssembly.Module;
const Instance = WebAssembly.Instance;
const Table = WebAssembly.Table;

function normalize(stack)
{
    var wasmFrameTypes = [
        {re:/^slow entry trampoline \(in wasm\)$/,                   sub:">"},
        {re:/^wasm-function\[(\d+)\] \(.*\)$/,                       sub:"$1"},
        {re:/^(fast|slow) FFI trampoline (to native )?\(in wasm\)$/, sub:"<"},
        {re:/^call to[ asm.js]? native (.*) \(in wasm\)$/,           sub:"$1"},
        {re:/ \(in wasm\)$/,                                         sub:""}
    ];

    var framesIn = stack.split(',');
    var framesOut = [];
    for (let frame of framesIn) {
        for (let {re, sub} of wasmFrameTypes) {
            if (re.test(frame)) {
                framesOut.push(frame.replace(re, sub));
                break;
            }
        }
    }

    return framesOut.join(',');
}

function removeAdjacentDuplicates(array) {
    if (array.length < 2)
        return;
    let i = 0;
    for (let j = 1; j < array.length; j++) {
        if (array[i] !== array[j])
            array[++i] = array[j];
    }
    array.length = i + 1;
}

function assertEqStacks(got, expect)
{
    for (let i = 0; i < got.length; i++)
        got[i] = normalize(got[i]);

    removeAdjacentDuplicates(got);

    if (got.length != expect.length) {
        print(`Got:\n${got.toSource()}\nExpect:\n${expect.toSource()}`);
        assertEq(got.length, expect.length);
    }

    for (let i = 0; i < got.length; i++) {
        if (got[i] !== expect[i]) {
            print(`On stack ${i}, Got:\n${got[i]}\nExpect:\n${expect[i]}`);
            assertEq(got[i], expect[i]);
        }
    }
}

function test(code, importObj, expect)
{
    enableGeckoProfiling();

    var f = wasmEvalText(code, importObj).exports[""];
    enableSingleStepProfiling();
    f();
    assertEqStacks(disableSingleStepProfiling(), expect);

    disableGeckoProfiling();
}

test(
`(module
    (func (result i32) (i32.const 42))
    (export "" 0)
)`,
{},
["", ">", "0,>", ">", ""]);

test(
`(module
    (func (result i32) (i32.add (call 1) (i32.const 1)))
    (func (result i32) (i32.const 42))
    (export "" 0)
)`,
{},
["", ">", "0,>", "1,0,>", "0,>", ">", ""]);

test(
`(module
    (func $foo (call_indirect 0 (i32.const 0)))
    (func $bar)
    (table anyfunc (elem $bar))
    (export "" $foo)
)`,
{},
["", ">", "0,>", "1,0,>", "0,>", ">", ""]);

test(
`(module
    (import $foo "" "foo")
    (table anyfunc (elem $foo))
    (func $bar (call_indirect 0 (i32.const 0)))
    (export "" $bar)
)`,
{"":{foo:()=>{}}},
["", ">", "1,>", "0,1,>", "<,0,1,>", "0,1,>", "1,>", ">", ""]);

test(`(module
    (import $f32 "Math" "sin" (param f32) (result f32))
    (func (export "") (param f32) (result f32)
        get_local 0
        call $f32
    )
)`,
this,
["", ">", "1,>", "<,1,>", "1,>", ">", ""]);

if (getBuildConfiguration()["arm-simulator"]) {
    // On ARM, some int64 operations are calls to C++.
    for (let op of ['div_s', 'rem_s', 'div_u', 'rem_u']) {
        test(`(module
            (func (export "") (param i32) (result i32)
                get_local 0
                i64.extend_s/i32
                i64.const 0x1a2b3c4d5e6f
                i64.${op}
                i32.wrap/i64
            )
        )`,
        this,
        ["", ">", "0,>", "<,0,>", `i64.${op},0,>`, "<,0,>", "0,>", ">", ""]);
    }
}

// current_memory is a callout.
test(`(module
    (memory 1)
    (func (export "") (result i32)
         current_memory
    )
)`,
this,
["", ">", "0,>", "<,0,>", "current_memory,0,>", "<,0,>", "0,>", ">", ""]);

// grow_memory is a callout.
test(`(module
    (memory 1)
    (func (export "") (result i32)
         i32.const 1
         grow_memory
    )
)`,
this,
["", ">", "0,>", "<,0,>", "grow_memory,0,>", "<,0,>", "0,>", ">", ""]);

// A few math builtins.
for (let type of ['f32', 'f64']) {
    for (let func of ['ceil', 'floor', 'nearest', 'trunc']) {
        test(`(module
            (func (export "") (param ${type}) (result ${type})
                get_local 0
                ${type}.${func}
            )
        )`,
        this,
        ["", ">", "0,>", "<,0,>", `${type}.${func},0,>`, "<,0,>", "0,>", ">", ""]);
    }
}

(function() {
    // Error handling.
    function testError(code, error, expect)
    {
        enableGeckoProfiling();
        var f = wasmEvalText(code).exports[""];
        enableSingleStepProfiling();
        assertThrowsInstanceOf(f, error);
        assertEqStacks(disableSingleStepProfiling(), expect);
        disableGeckoProfiling();
    }

    testError(
    `(module
        (func $foo (unreachable))
        (func (export "") (call $foo))
    )`,
    WebAssembly.RuntimeError,
    ["", ">", "1,>", "0,1,>", "1,>", "", ">", ""]);

    testError(
    `(module
        (type $good (func))
        (type $bad (func (param i32)))
        (func $foo (call_indirect $bad (i32.const 1) (i32.const 0)))
        (func $bar (type $good))
        (table anyfunc (elem $bar))
        (export "" $foo)
    )`,
    WebAssembly.RuntimeError,
    // Technically we have this one *one-instruction* interval where
    // the caller is lost (the stack with "1,>"). It's annoying to fix and shouldn't
    // mess up profiles in practice so we ignore it.
    ["", ">", "0,>", "1,0,>", "1,>", "trap handling,0,>", "", ">", ""]);
})();

(function() {
    // Tables fun.
    var e = wasmEvalText(`
    (module
        (func $foo (result i32) (i32.const 42))
        (export "foo" $foo)
        (func $bar (result i32) (i32.const 13))
        (table 10 anyfunc)
        (elem (i32.const 0) $foo $bar)
        (export "tbl" table)
    )`).exports;
    assertEq(e.foo(), 42);
    assertEq(e.tbl.get(0)(), 42);
    assertEq(e.tbl.get(1)(), 13);

    enableGeckoProfiling();
    enableSingleStepProfiling();
    assertEq(e.tbl.get(0)(), 42);
    assertEqStacks(disableSingleStepProfiling(), ["", ">", "0,>", ">", ""]);
    disableGeckoProfiling();

    assertEq(e.foo(), 42);
    assertEq(e.tbl.get(0)(), 42);
    assertEq(e.tbl.get(1)(), 13);

    enableGeckoProfiling();
    enableSingleStepProfiling();
    assertEq(e.tbl.get(1)(), 13);
    assertEqStacks(disableSingleStepProfiling(), ["", ">", "1,>", ">", ""]);
    disableGeckoProfiling();

    assertEq(e.tbl.get(0)(), 42);
    assertEq(e.tbl.get(1)(), 13);
    assertEq(e.foo(), 42);

    enableGeckoProfiling();
    enableSingleStepProfiling();
    assertEq(e.foo(), 42);
    assertEq(e.tbl.get(1)(), 13);
    assertEqStacks(disableSingleStepProfiling(), ["", ">", "0,>", ">", "", ">", "1,>", ">", ""]);
    disableGeckoProfiling();

    var e2 = wasmEvalText(`
    (module
        (type $v2i (func (result i32)))
        (import "a" "b" (table 10 anyfunc))
        (elem (i32.const 2) $bar)
        (func $bar (result i32) (i32.const 99))
        (func $baz (param $i i32) (result i32) (call_indirect $v2i (get_local $i)))
        (export "baz" $baz)
    )`, {a:{b:e.tbl}}).exports;

    enableGeckoProfiling();
    enableSingleStepProfiling();
    assertEq(e2.baz(0), 42);
    assertEqStacks(disableSingleStepProfiling(), ["", ">", "1,>", "0,1,>", "1,>", ">", ""]);
    disableGeckoProfiling();

    enableGeckoProfiling();
    enableSingleStepProfiling();
    assertEq(e2.baz(1), 13);
    assertEqStacks(disableSingleStepProfiling(), ["", ">", "1,>", "1,1,>", "1,>", ">", ""]);
    disableGeckoProfiling();

    enableGeckoProfiling();
    enableSingleStepProfiling();
    assertEq(e2.baz(2), 99);
    assertEqStacks(disableSingleStepProfiling(), ["", ">", "1,>", "0,1,>", "1,>", ">", ""]);
    disableGeckoProfiling();
})();

(function() {
    // Optimized wasm->wasm import.
    var m1 = new Module(wasmTextToBinary(`(module
        (func $foo (result i32) (i32.const 42))
        (export "foo" $foo)
    )`));
    var m2 = new Module(wasmTextToBinary(`(module
        (import $foo "a" "foo" (result i32))
        (func $bar (result i32) (call $foo))
        (export "bar" $bar)
    )`));

    // Instantiate while not active:
    var e1 = new Instance(m1).exports;
    var e2 = new Instance(m2, {a:e1}).exports;
    enableGeckoProfiling();
    enableSingleStepProfiling();
    assertEq(e2.bar(), 42);
    assertEqStacks(disableSingleStepProfiling(), ["", ">", "1,>", "0,1,>", "1,>", ">", ""]);
    disableGeckoProfiling();
    assertEq(e2.bar(), 42);

    // Instantiate while active:
    enableGeckoProfiling();
    var e3 = new Instance(m1).exports;
    var e4 = new Instance(m2, {a:e3}).exports;
    enableSingleStepProfiling();
    assertEq(e4.bar(), 42);
    assertEqStacks(disableSingleStepProfiling(), ["", ">", "1,>", "0,1,>", "1,>", ">", ""]);
    disableGeckoProfiling();
    assertEq(e4.bar(), 42);
})();

(function() {
    // FFIs test.
    let prevOptions = getJitCompilerOptions();

    // Skip tests if baseline isn't enabled, since the stacks might differ by
    // a few instructions.
    if (prevOptions['baseline.enable'] === 0)
        return;

    setJitCompilerOption("baseline.warmup.trigger", 10);

    enableGeckoProfiling();

    var m = new Module(wasmTextToBinary(`(module
        (import $ffi "a" "ffi" (param i32) (result i32))

        (import $missingOneArg "a" "sumTwo" (param i32) (result i32))

        (func (export "foo") (param i32) (result i32)
         get_local 0
         call $ffi)

        (func (export "id") (param i32) (result i32)
         get_local 0
         call $missingOneArg
        )
    )`));

    var valueToConvert = 0;
    function ffi(n) {
        new Error().stack; // enter VM to clobber FP register.
        if (n == 1337) { return valueToConvert };
        return 42;
    }

    function sumTwo(a, b) {
        return (a|0)+(b|0)|0;
    }

    // Baseline compile ffi.
    for (var i = 20; i --> 0;) {
        ffi(i);
        sumTwo(i-1, i+1);
    }

    var imports = {
        a: {
            ffi,
            sumTwo
        }
    };

    var i = new Instance(m, imports).exports;

    // Enable the jit exit.
    assertEq(i.foo(0), 42);
    assertEq(i.id(13), 13);

    // Test normal conditions.
    enableSingleStepProfiling();
    assertEq(i.foo(0), 42);
    assertEqStacks(disableSingleStepProfiling(), ["", ">", "2,>", "<,2,>",
        // Losing stack information while the JIT func prologue sets profiler
        // virtual FP.
        "",
        // Callee time.
        "<,2,>",
        // Losing stack information while we're exiting JIT func epilogue and
        // recovering wasm FP.
        "",
        // Back into the jit exit (frame info has been recovered).
        "<,2,>",
        // Normal unwinding.
        "2,>", ">", ""]);

    // Test rectifier frame.
    enableSingleStepProfiling();
    assertEq(i.id(100), 100);
    assertEqStacks(disableSingleStepProfiling(), ["", ">", "3,>", "<,3,>",
        // Rectifier frame time is spent here (lastProfilingFrame has not been
        // set).
        "",
        "<,3,>",
        // Rectifier frame unwinding time is spent here.
        "",
        "<,3,>",
        "3,>", ">", ""]);

    // Test OOL coercion path.
    valueToConvert = 2**31;

    enableSingleStepProfiling();
    assertEq(i.foo(1337), -(2**31));
    assertEqStacks(disableSingleStepProfiling(), ["", ">", "2,>", "<,2,>", "", "<,2,>", "",
        // Back into the jit exit (frame info has been recovered).
        // Inline conversion fails, we skip to the OOL path, call from there
        // and get back to the jit exit.
        "<,2,>",
        // Normal unwinding.
        "2,>", ">", ""]);

    disableGeckoProfiling();
    setJitCompilerOption("baseline.warmup.trigger", prevOptions["baseline.warmup.trigger"]);
})();

// Make sure it's possible to single-step through call through debug-enabled code.
(function() {
 enableGeckoProfiling();

 let g = newGlobal('');
 let dbg = new Debugger(g);
 dbg.onEnterFrame = () => {};
 enableSingleStepProfiling();
 g.eval(`
    var code = wasmTextToBinary('(module (func (export "run") (result i32) i32.const 42))');
    var i = new WebAssembly.Instance(new WebAssembly.Module(code));
    assertEq(i.exports.run(), 42);
 `);

 disableSingleStepProfiling();
 disableGeckoProfiling();
})();
