// |jit-test| slow;

if (typeof oomTest === 'undefined')
    quit();

// Test baseline compiler only.
if (typeof wasmCompileMode === 'undefined' || wasmCompileMode() != 'baseline')
    quit();

try {
    var bin = wasmTextToBinary(
	`(module (func (result i32) (param f64) (param f32)
                i64.const 0
                get_local 0
                drop
                i32.wrap/i64
                f64.const 0
                f64.const 0
                i32.const 0
                select
                f32.const 0
                f32.const 0
                f32.const 0
                i32.const 0
                select
                i32.const 0
                i32.const 0
                i32.const 0
                select
                select
                drop
                  drop))`);
    oomTest(() => new WebAssembly.Module(bin));
} catch(e) { }
