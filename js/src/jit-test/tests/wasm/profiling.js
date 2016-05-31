load(libdir + "wasm.js");
load(libdir + "asserts.js");

// Single-step profiling currently only works in the ARM simulator
if (!getBuildConfiguration()["arm-simulator"])
    quit();

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

function assertEqStacks(got, expect)
{
    for (let i = 0; i < got.length; i++)
        got[i] = normalize(got[i]);

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

    var f = wasmEvalText(code);
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
    (table $bar)
    (export "" $foo)
)`,
["", ">", "0,>", "1,0,>", "0,>", ">", ""]);

function testError(code, error)
{
    enableSPSProfiling();
    var f = wasmEvalText(code);
    enableSingleStepProfiling();
    assertThrowsInstanceOf(f, error);
    disableSingleStepProfiling();
    disableSPSProfiling();
}

testError(
`(module
    (type $good (func))
    (type $bad (func (param i32)))
    (func $foo (call_indirect $bad (i32.const 0) (i32.const 1)))
    (func $bar (type $good))
    (table $bar)
    (export "" $foo)
)`,
Error);
