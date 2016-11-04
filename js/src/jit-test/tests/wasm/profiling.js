load(libdir + "wasm.js");

// Single-step profiling currently only works in the ARM simulator
if (!getBuildConfiguration()["arm-simulator"])
    quit();

const Module = WebAssembly.Module;
const Instance = WebAssembly.Instance;
const Table = WebAssembly.Table;

function normalize(stack)
{
    var wasmFrameTypes = [
        {re:/^entry trampoline \(in asm.js\)$/,             sub:">"},
        {re:/^wasm-function\[(\d+)\] \(.*\)$/,              sub:"$1"},
        {re:/^(fast|slow) FFI trampoline \(in asm.js\)$/,   sub:"<"},
        {re:/ \(in asm.js\)$/,                              sub:""}
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

function test(code, expect)
{
    enableSPSProfiling();

    var f = wasmEvalText(code).exports[""];
    enableSingleStepProfiling();
    f();
    assertEqStacks(disableSingleStepProfiling(), expect);

    disableSPSProfiling();
}

test(
`(module
    (func (result i32) (i32.const 42))
    (export "" 0)
)`,
["", ">", "0,>", ">", ""]);

test(
`(module
    (func (result i32) (i32.add (call 1) (i32.const 1)))
    (func (result i32) (i32.const 42))
    (export "" 0)
)`,
["", ">", "0,>", "1,0,>", "0,>", ">", ""]);

test(
`(module
    (func $foo (call_indirect 0 (i32.const 0)))
    (func $bar)
    (table anyfunc (elem $bar))
    (export "" $foo)
)`,
["", ">", "0,>", "1,0,>", "0,>", ">", ""]);

function testError(code, error, expect)
{
    enableSPSProfiling();
    var f = wasmEvalText(code).exports[""];
    enableSingleStepProfiling();
    assertThrowsInstanceOf(f, error);
    assertEqStacks(disableSingleStepProfiling(), expect);
    disableSPSProfiling();
}

testError(
`(module
    (func $foo (unreachable))
    (func (export "") (call $foo))
)`,
WebAssembly.RuntimeError,
["", ">", "1,>", "0,1,>", "trap handling,0,1,>", "inline stub,0,1,>", ""]);

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
["", ">", "0,>", "1,0,>", "1,>", "trap handling,0,>", "inline stub,0,>", ""]);

(function() {
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

    enableSPSProfiling();
    enableSingleStepProfiling();
    assertEq(e.tbl.get(0)(), 42);
    assertEqStacks(disableSingleStepProfiling(), ["", ">", "0,>", ">", ""]);
    disableSPSProfiling();

    assertEq(e.foo(), 42);
    assertEq(e.tbl.get(0)(), 42);
    assertEq(e.tbl.get(1)(), 13);

    enableSPSProfiling();
    enableSingleStepProfiling();
    assertEq(e.tbl.get(1)(), 13);
    assertEqStacks(disableSingleStepProfiling(), ["", ">", "1,>", ">", ""]);
    disableSPSProfiling();

    assertEq(e.tbl.get(0)(), 42);
    assertEq(e.tbl.get(1)(), 13);
    assertEq(e.foo(), 42);

    enableSPSProfiling();
    enableSingleStepProfiling();
    assertEq(e.foo(), 42);
    assertEq(e.tbl.get(1)(), 13);
    assertEqStacks(disableSingleStepProfiling(), ["", ">", "0,>", ">", "", ">", "1,>", ">", ""]);
    disableSPSProfiling();

    var e2 = wasmEvalText(`
    (module
        (type $v2i (func (result i32)))
        (import "a" "b" (table 10 anyfunc))
        (elem (i32.const 2) $bar)
        (func $bar (result i32) (i32.const 99))
        (func $baz (param $i i32) (result i32) (call_indirect $v2i (get_local $i)))
        (export "baz" $baz)
    )`, {a:{b:e.tbl}}).exports;

    enableSPSProfiling();
    enableSingleStepProfiling();
    assertEq(e2.baz(0), 42);
    assertEqStacks(disableSingleStepProfiling(), ["", ">", "1,>", "0,1,>", "1,>", ">", ""]);
    disableSPSProfiling();

    enableSPSProfiling();
    enableSingleStepProfiling();
    assertEq(e2.baz(1), 13);
    assertEqStacks(disableSingleStepProfiling(), ["", ">", "1,>", "1,1,>", "1,>", ">", ""]);
    disableSPSProfiling();

    enableSPSProfiling();
    enableSingleStepProfiling();
    assertEq(e2.baz(2), 99);
    assertEqStacks(disableSingleStepProfiling(), ["", ">", "1,>", "0,1,>", "1,>", ">", ""]);
    disableSPSProfiling();
})();

(function() {
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
    enableSPSProfiling();
    enableSingleStepProfiling();
    assertEq(e2.bar(), 42);
    assertEqStacks(disableSingleStepProfiling(), ["", ">", "1,>", "0,1,>", "1,>", ">", ""]);
    disableSPSProfiling();
    assertEq(e2.bar(), 42);

    // Instantiate while active:
    enableSPSProfiling();
    var e3 = new Instance(m1).exports;
    var e4 = new Instance(m2, {a:e3}).exports;
    enableSingleStepProfiling();
    assertEq(e4.bar(), 42);
    assertEqStacks(disableSingleStepProfiling(), ["", ">", "1,>", "0,1,>", "1,>", ">", ""]);
    disableSPSProfiling();
    assertEq(e4.bar(), 42);
})();
