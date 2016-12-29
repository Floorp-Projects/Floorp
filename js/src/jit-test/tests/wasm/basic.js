load(libdir + "wasm.js");

const LinkError = WebAssembly.LinkError;

// ----------------------------------------------------------------------------
// exports

var o = wasmEvalText('(module)').exports;
assertEq(Object.getOwnPropertyNames(o).length, 0);

var o = wasmEvalText('(module (func))').exports;
assertEq(Object.getOwnPropertyNames(o).length, 0);

var o = wasmEvalText('(module (func) (export "a" 0))').exports;
var names = Object.getOwnPropertyNames(o);
assertEq(names.length, 1);
assertEq(names[0], 'a');
var desc = Object.getOwnPropertyDescriptor(o, 'a');
assertEq(typeof desc.value, "function");
assertEq(desc.value.name, "0");
assertEq(desc.value.length, 0);
assertEq(desc.value(), undefined);
assertEq(desc.writable, false);
assertEq(desc.enumerable, true);
assertEq(desc.configurable, false);
assertEq(desc.value(), undefined);

wasmValidateText('(module (func) (func) (export "a" 0))');
wasmValidateText('(module (func) (func) (export "a" 1))');
wasmValidateText('(module (func $a) (func $b) (export "a" $a) (export "b" $b))');
wasmValidateText('(module (func $a) (func $b) (export "a" $a) (export "b" $b))');

wasmFailValidateText('(module (func) (export "a" 1))', /exported function index out of bounds/);
wasmFailValidateText('(module (func) (func) (export "a" 2))', /exported function index out of bounds/);

var o = wasmEvalText('(module (func) (export "a" 0) (export "b" 0))').exports;
assertEq(Object.getOwnPropertyNames(o).sort().toString(), "a,b");
assertEq(o.a.name, "0");
assertEq(o.b.name, "0");
assertEq(o.a === o.b, true);

var o = wasmEvalText('(module (func) (func) (export "a" 0) (export "b" 1))').exports;
assertEq(Object.getOwnPropertyNames(o).sort().toString(), "a,b");
assertEq(o.a.name, "0");
assertEq(o.b.name, "1");
assertEq(o.a === o.b, false);

var o = wasmEvalText('(module (func (result i32) (i32.const 1)) (func (result i32) (i32.const 2)) (export "a" 0) (export "b" 1))').exports;
assertEq(o.a(), 1);
assertEq(o.b(), 2);
var o = wasmEvalText('(module (func (result i32) (i32.const 1)) (func (result i32) (i32.const 2)) (export "a" 1) (export "b" 0))').exports;
assertEq(o.a(), 2);
assertEq(o.b(), 1);

wasmFailValidateText('(module (func) (export "a" 0) (export "a" 0))', /duplicate export/);
wasmFailValidateText('(module (func) (func) (export "a" 0) (export "a" 1))', /duplicate export/);

// ----------------------------------------------------------------------------
// signatures

wasmFailValidateText('(module (func (result i32)))', mismatchError("void", "i32"));
wasmFailValidateText('(module (func (result i32) (nop)))', mismatchError("void", "i32"));

wasmValidateText('(module (func (nop)))');
wasmValidateText('(module (func (result i32) (i32.const 42)))');
wasmValidateText('(module (func (param i32)))');
wasmValidateText('(module (func (param i32) (result i32) (i32.const 42)))');
wasmValidateText('(module (func (result i32) (param i32) (i32.const 42)))');
wasmValidateText('(module (func (param f32)))');
wasmValidateText('(module (func (param f64)))');

var f = wasmEvalText('(module (func (param i64) (result i32) (i32.const 123)) (export "" 0))').exports[""];
assertErrorMessage(f, TypeError, /i64/);
var f = wasmEvalText('(module (func (param i32) (result i64) (i64.const 123)) (export "" 0))').exports[""];
assertErrorMessage(f, TypeError, /i64/);

var f = wasmEvalText('(module (import $imp "a" "b" (param i64) (result i32)) (func $f (result i32) (call $imp (i64.const 0))) (export "" $f))', {a:{b:()=>{}}}).exports[""];
assertErrorMessage(f, TypeError, /i64/);
var f = wasmEvalText('(module (import $imp "a" "b" (result i64)) (func $f (result i64) (call $imp)) (export "" $f))', {a:{b:()=>{}}}).exports[""];
assertErrorMessage(f, TypeError, /i64/);

setJitCompilerOption('wasm.test-mode', 1);
wasmFullPassI64('(module (func (result i64) (i64.const 123)) (export "run" 0))', {low: 123, high: 0});
wasmFullPassI64('(module (func (param i64) (result i64) (get_local 0)) (export "run" 0))',
                { low: 0x7fffffff, high: 0x12340000},
                {},
                {low: 0x7fffffff, high: 0x12340000});
wasmFullPassI64('(module (func (param i64) (result i64) (i64.add (get_local 0) (i64.const 1))) (export "run" 0))',
                {low: 0x0, high: 0x12340001},
                {},
                { low: 0xffffffff, high: 0x12340000});
setJitCompilerOption('wasm.test-mode', 0);

// ----------------------------------------------------------------------------
// imports

const noImportObj = "second argument must be an object";

assertErrorMessage(() => wasmEvalText('(module (import "a" "b"))', 1), TypeError, noImportObj);
assertErrorMessage(() => wasmEvalText('(module (import "a" "b"))', null), TypeError, noImportObj);

const notObject = /import object field '\w*' is not an Object/;
const notFunction = /import object field '\w*' is not a Function/;

var code = '(module (import "a" "b"))';
assertErrorMessage(() => wasmEvalText(code), TypeError, noImportObj);
assertErrorMessage(() => wasmEvalText(code, {}), TypeError, notObject);
assertErrorMessage(() => wasmEvalText(code, {a:1}), TypeError, notObject);
assertErrorMessage(() => wasmEvalText(code, {a:{}}), LinkError, notFunction);
assertErrorMessage(() => wasmEvalText(code, {a:{b:1}}), LinkError, notFunction);
wasmEvalText(code, {a:{b:()=>{}}});

var code = '(module (import "" "b"))';
wasmEvalText(code, {"":{b:()=>{}}});

var code = '(module (import "a" ""))';
assertErrorMessage(() => wasmEvalText(code), TypeError, noImportObj);
assertErrorMessage(() => wasmEvalText(code, {}), TypeError, notObject);
assertErrorMessage(() => wasmEvalText(code, {a:1}), TypeError, notObject);
wasmEvalText(code, {a:{"":()=>{}}});

var code = '(module (import "a" "") (import "b" "c") (import "c" ""))';
assertErrorMessage(() => wasmEvalText(code, {a:()=>{}, b:{c:()=>{}}, c:{}}), LinkError, notFunction);
wasmEvalText(code, {a:{"":()=>{}}, b:{c:()=>{}}, c:{"":()=>{}}});

wasmEvalText('(module (import "a" "" (result i32)))', {a:{"":()=>{}}});
wasmEvalText('(module (import "a" "" (result f32)))', {a:{"":()=>{}}});
wasmEvalText('(module (import "a" "" (result f64)))', {a:{"":()=>{}}});
wasmEvalText('(module (import $foo "a" "" (result f64)))', {a:{"":()=>{}}});

// ----------------------------------------------------------------------------
// memory

wasmValidateText('(module (memory 0))');
wasmValidateText('(module (memory 1))');
wasmFailValidateText('(module (memory 65536))', /initial memory size too big/);

// May OOM, but must not crash:
try {
    wasmEvalText('(module (memory 65535))');
} catch (e) {
    assertEq(String(e).indexOf("out of memory") != -1 ||
             String(e).indexOf("memory size too big") != -1, true);
}

var buf = wasmEvalText('(module (memory 1) (export "memory" memory))').exports.memory.buffer;
assertEq(buf instanceof ArrayBuffer, true);
assertEq(buf.byteLength, 65536);

var obj = wasmEvalText('(module (memory 1) (func (result i32) (i32.const 42)) (func (nop)) (export "memory" memory) (export "b" 0) (export "c" 1))').exports;
assertEq(obj.memory.buffer instanceof ArrayBuffer, true);
assertEq(obj.b instanceof Function, true);
assertEq(obj.c instanceof Function, true);
assertEq(obj.memory.buffer.byteLength, 65536);
assertEq(obj.b(), 42);
assertEq(obj.c(), undefined);

var buf = wasmEvalText('(module (memory 1) (data (i32.const 0) "") (export "memory" memory))').exports.memory.buffer;
assertEq(new Uint8Array(buf)[0], 0);

var buf = wasmEvalText('(module (memory 1) (data (i32.const 65536) "") (export "memory" memory))').exports.memory.buffer;
assertEq(new Uint8Array(buf)[0], 0);

var buf = wasmEvalText('(module (memory 1) (data (i32.const 0) "a") (export "memory" memory))').exports.memory.buffer;
assertEq(new Uint8Array(buf)[0], 'a'.charCodeAt(0));

var buf = wasmEvalText('(module (memory 1) (data (i32.const 0) "a") (data (i32.const 2) "b") (export "memory" memory))').exports.memory.buffer;
assertEq(new Uint8Array(buf)[0], 'a'.charCodeAt(0));
assertEq(new Uint8Array(buf)[1], 0);
assertEq(new Uint8Array(buf)[2], 'b'.charCodeAt(0));

var buf = wasmEvalText('(module (memory 1) (data (i32.const 65535) "c") (export "memory" memory))').exports.memory.buffer;
assertEq(new Uint8Array(buf)[0], 0);
assertEq(new Uint8Array(buf)[65535], 'c'.charCodeAt(0));

// ----------------------------------------------------------------------------
// locals

assertEq(wasmEvalText('(module (func (param i32) (result i32) (get_local 0)) (export "" 0))').exports[""](), 0);
assertEq(wasmEvalText('(module (func (param i32) (result i32) (get_local 0)) (export "" 0))').exports[""](42), 42);
assertEq(wasmEvalText('(module (func (param i32) (param i32) (result i32) (get_local 0)) (export "" 0))').exports[""](42, 43), 42);
assertEq(wasmEvalText('(module (func (param i32) (param i32) (result i32) (get_local 1)) (export "" 0))').exports[""](42, 43), 43);

wasmFailValidateText('(module (func (get_local 0)))', /get_local index out of range/);
wasmFailValidateText('(module (func (result f32) (local i32) (get_local 0)))', mismatchError("i32", "f32"));
wasmFailValidateText('(module (func (result i32) (local f32) (get_local 0)))', mismatchError("f32", "i32"));
wasmFailValidateText('(module (func (result f32) (param i32) (local f32) (get_local 0)))', mismatchError("i32", "f32"));
wasmFailValidateText('(module (func (result i32) (param i32) (local f32) (get_local 1)))', mismatchError("f32", "i32"));

wasmValidateText('(module (func (local i32)))');
wasmValidateText('(module (func (local i32) (local f32)))');

wasmFullPass('(module (func (result i32) (local i32) (get_local 0)) (export "run" 0))', 0);
wasmFullPass('(module (func (result i32) (param i32) (local f32) (get_local 0)) (export "run" 0))', 0);
wasmFullPass('(module (func (result f32) (param i32) (local f32) (get_local 1)) (export "run" 0))', 0);

wasmFailValidateText('(module (func (set_local 0 (i32.const 0))))', /set_local index out of range/);
wasmFailValidateText('(module (func (local f32) (set_local 0 (i32.const 0))))', mismatchError("i32", "f32"));
wasmFailValidateText('(module (func (local f32) (set_local 0 (nop))))', /popping value from empty stack/);
wasmFailValidateText('(module (func (local i32) (local f32) (set_local 0 (get_local 1))))', mismatchError("f32", "i32"));
wasmFailValidateText('(module (func (local i32) (local f32) (set_local 1 (get_local 0))))', mismatchError("i32", "f32"));

wasmValidateText('(module (func (local i32) (set_local 0 (i32.const 0))))');
wasmValidateText('(module (func (local i32) (local f32) (set_local 0 (get_local 0))))');
wasmValidateText('(module (func (local i32) (local f32) (set_local 1 (get_local 1))))');

wasmFullPass('(module (func (result i32) (local i32) (tee_local 0 (i32.const 42))) (export "run" 0))', 42);
wasmFullPass('(module (func (result i32) (local i32) (tee_local 0 (get_local 0))) (export "run" 0))', 0);

wasmFullPass('(module (func (param $a i32) (result i32) (get_local $a)) (export "run" 0))', 0);
wasmFullPass('(module (func (param $a i32) (local $b i32) (result i32) (block i32 (set_local $b (get_local $a)) (get_local $b))) (export "run" 0))', 42, {}, 42);

wasmValidateText('(module (func (local i32) (local $a f32) (set_local 0 (i32.const 1)) (set_local $a (f32.const nan))))');

// ----------------------------------------------------------------------------
// blocks

wasmFullPass('(module (func (block )) (export "run" 0))', undefined);

wasmFailValidateText('(module (func (result i32) (block )))', mismatchError("void", "i32"));
wasmFailValidateText('(module (func (result i32) (block (block ))))', mismatchError("void", "i32"));
wasmFailValidateText('(module (func (local i32) (set_local 0 (block ))))', /popping value from empty stack/);

wasmFullPass('(module (func (block (block ))) (export "run" 0))', undefined);
wasmFullPass('(module (func (result i32) (block i32 (i32.const 42))) (export "run" 0))', 42);
wasmFullPass('(module (func (result i32) (block i32 (block i32 (i32.const 42)))) (export "run" 0))', 42);
wasmFailValidateText('(module (func (result f32) (block i32 (i32.const 0))))', mismatchError("i32", "f32"));

wasmFullPass('(module (func (result i32) (block i32 (drop (i32.const 13)) (block i32 (i32.const 42)))) (export "run" 0))', 42);
wasmFailValidateText('(module (func (result f32) (param f32) (block i32 (drop (get_local 0)) (i32.const 0))))', mismatchError("i32", "f32"));

wasmFullPass('(module (func (result i32) (local i32) (set_local 0 (i32.const 42)) (get_local 0)) (export "run" 0))', 42);

// ----------------------------------------------------------------------------
// calls

wasmFailValidateText('(module (func (nop)) (func (call 0 (i32.const 0))))', /unused values not explicitly dropped by end of block/);

wasmFailValidateText('(module (func (param i32) (nop)) (func (call 0)))', /peeking at value from outside block/);
wasmFailValidateText('(module (func (param f32) (nop)) (func (call 0 (i32.const 0))))', mismatchError("i32", "f32"));
wasmFailValidateText('(module (func (nop)) (func (call 3)))', /callee index out of range/);

wasmValidateText('(module (func (nop)) (func (call 0)))');
wasmValidateText('(module (func (param i32) (nop)) (func (call 0 (i32.const 0))))');

wasmFullPass('(module (func (result i32) (i32.const 42)) (func (result i32) (call 0)) (export "run" 1))', 42);
assertThrowsInstanceOf(() => wasmEvalText('(module (func (call 0)) (export "" 0))').exports[""](), InternalError);
assertThrowsInstanceOf(() => wasmEvalText('(module (func (call 1)) (func (call 0)) (export "" 0))').exports[""](), InternalError);

wasmValidateText('(module (func (param i32 f32)) (func (call 0 (i32.const 0) (f32.const nan))))');
wasmFailValidateText('(module (func (param i32 f32)) (func (call 0 (i32.const 0) (i32.const 0))))', mismatchError("i32", "f32"));

wasmFailValidateText('(module (import "a" "") (func (call 0 (i32.const 0))))', /unused values not explicitly dropped by end of block/);
wasmFailValidateText('(module (import "a" "" (param i32)) (func (call 0)))', /peeking at value from outside block/);
wasmFailValidateText('(module (import "a" "" (param f32)) (func (call 0 (i32.const 0))))', mismatchError("i32", "f32"));

assertErrorMessage(() => wasmEvalText('(module (import "a" "") (func (call 1)))'), TypeError, noImportObj);
wasmEvalText('(module (import "" "a") (func (call 0)))', {"":{a:()=>{}}});
wasmEvalText('(module (import "" "a" (param i32)) (func (call 0 (i32.const 0))))', {"":{a:()=>{}}});

function checkF32CallImport(v) {
    wasmFullPass('(module (import "" "a" (result f32)) (func (result f32) (call 0)) (export "run" 1))',
                 Math.fround(v),
                 {"":{a:()=>{ return v; }}});
    wasmFullPass('(module (import "" "a" (param f32)) (func (param f32) (call 0 (get_local 0))) (export "run" 1))',
                 undefined,
                 {"":{a:x=>{ assertEq(Math.fround(v), x); }}},
                 v);
}
checkF32CallImport(13.37);
checkF32CallImport(NaN);
checkF32CallImport(-Infinity);
checkF32CallImport(-0);
checkF32CallImport(Math.pow(2, 32) - 1);

var counter = 0;
var f = wasmEvalText('(module (import "" "inc") (func (call 0)) (export "" 1))', {"":{inc:()=>counter++}}).exports[""];
var g = wasmEvalText('(module (import "" "f") (func (block (call 0) (call 0))) (export "" 1))', {"":{f}}).exports[""];
f();
assertEq(counter, 1);
g();
assertEq(counter, 3);

var f = wasmEvalText('(module (import "" "callf") (func (call 0)) (export "" 1))', {"":{callf:()=>f()}}).exports[""];
assertThrowsInstanceOf(() => f(), InternalError);

var f = wasmEvalText('(module (import "" "callg") (func (call 0)) (export "" 1))', {"":{callg:()=>g()}}).exports[""];
var g = wasmEvalText('(module (import "" "callf") (func (call 0)) (export "" 1))', {"":{callf:()=>f()}}).exports[""];
assertThrowsInstanceOf(() => f(), InternalError);

var code = '(module (import "" "one" (result i32)) (import "" "two" (result i32)) (func (result i32) (i32.const 3)) (func (result i32) (i32.const 4)) (func (result i32) BODY) (export "run" 4))';
var imports = {"":{one:()=>1, two:()=>2}};
wasmFullPass(code.replace('BODY', '(call 0)'), 1, imports);
wasmFullPass(code.replace('BODY', '(call 1)'), 2, imports);
wasmFullPass(code.replace('BODY', '(call 2)'), 3, imports);
wasmFullPass(code.replace('BODY', '(call 3)'), 4, imports);

wasmFullPass(`(module (import "" "evalcx" (param i32) (result i32)) (func (result i32) (call 0 (i32.const 0))) (export "run" 1))`, 0, {"":{evalcx}});

if (typeof evaluate === 'function')
    evaluate(`new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary('(module)'))) `, { fileName: null });

{
    setJitCompilerOption('wasm.test-mode', 1);

    let imp = {"":{
        param(i64) {
            assertEqI64(i64, {
                low: 0x9abcdef0,
                high: 0x12345678
            });
            return 42;
        },
        result(i32) {
            return {
                low: 0xabcdef01,
                high: 0x12345678 + i32
            }
        },
        paramAndResult(i64) {
            assertEqI64(i64, {
                low: 0x9abcdef0,
                high: 0x12345678
            });
            i64.low = 1337;
            return i64;
        }
    }}

    wasmFullPass(`(module
        (import "" "param" (param i64) (result i32))
        (func (result i32) (call 0 (i64.const 0x123456789abcdef0)))
        (export "run" 1))`, 42, imp);

    wasmFullPass(`(module
        (import "" "param" (param i64)(param i64)(param i64)(param i64)(param i64)(param i64)(param i64) (param i64) (result i32))
        (func (result i32) (call 0 (i64.const 0x123456789abcdef0)(i64.const 0x123456789abcdef0)(i64.const 0x123456789abcdef0)(i64.const 0x123456789abcdef0)(i64.const 0x123456789abcdef0)(i64.const 0x123456789abcdef0)(i64.const 0x123456789abcdef0)(i64.const 0x123456789abcdef0)))
        (export "run" 1))`, 42, imp);

    wasmFullPassI64(`(module
        (import "" "result" (param i32) (result i64))
        (func (result i64) (call 0 (i32.const 3)))
        (export "run" 1))`, { low: 0xabcdef01, high: 0x1234567b }, imp);

    // Ensure the ion exit is never taken.
    let ionThreshold = 2 * getJitCompilerOptions()['ion.warmup.trigger'];
    wasmFullPassI64(`(module
        (import "" "paramAndResult" (param i64) (result i64))
        (func (result i64) (local i32) (local i64)
         (set_local 0 (i32.const 0))
         (loop $out $in
             (set_local 1 (call 0 (i64.const 0x123456789abcdef0)))
             (set_local 0 (i32.add (get_local 0) (i32.const 1)))
             (if (i32.le_s (get_local 0) (i32.const ${ionThreshold})) (br $in))
         )
         (get_local 1)
        )
    (export "run" 1))`, { low: 1337, high: 0x12345678 }, imp);

    wasmFullPassI64(`(module
        (import "" "paramAndResult" (param i64) (result i64))
        (func (result i64) (local i32) (local i64)
         (set_local 0 (i32.const 0))
         (block $out
             (loop $in
                 (set_local 1 (call 0 (i64.const 0x123456789abcdef0)))
                 (set_local 0 (i32.add (get_local 0) (i32.const 1)))
                 (if (i32.le_s (get_local 0) (i32.const ${ionThreshold})) (br $in))
             )
         )
         (get_local 1)
        )
    (export "run" 1))`, { low: 1337, high: 0x12345678 }, imp);

    setJitCompilerOption('wasm.test-mode', 0);
}

wasmFailValidateText(`(module (type $t (func)) (func (call_indirect $t (i32.const 0))))`, /can't call_indirect without a table/);

var {v2i, i2i, i2v} = wasmEvalText(`(module
    (type (func (result i32)))
    (type (func (param i32) (result i32)))
    (type (func (param i32)))
    (func (type 0) (i32.const 13))
    (func (type 0) (i32.const 42))
    (func (type 1) (i32.add (get_local 0) (i32.const 1)))
    (func (type 1) (i32.add (get_local 0) (i32.const 2)))
    (func (type 1) (i32.add (get_local 0) (i32.const 3)))
    (func (type 1) (i32.add (get_local 0) (i32.const 4)))
    (table anyfunc (elem 0 1 2 3 4 5))
    (func (param i32) (result i32) (call_indirect 0 (get_local 0)))
    (func (param i32) (param i32) (result i32) (call_indirect 1 (get_local 1) (get_local 0)))
    (func (param i32) (call_indirect 2 (i32.const 0) (get_local 0)))
    (export "v2i" 6)
    (export "i2i" 7)
    (export "i2v" 8)
)`).exports;

const signatureMismatch = /indirect call signature mismatch/;

assertEq(v2i(0), 13);
assertEq(v2i(1), 42);
assertErrorMessage(() => v2i(2), Error, signatureMismatch);
assertErrorMessage(() => v2i(3), Error, signatureMismatch);
assertErrorMessage(() => v2i(4), Error, signatureMismatch);
assertErrorMessage(() => v2i(5), Error, signatureMismatch);

assertErrorMessage(() => i2i(0), Error, signatureMismatch);
assertErrorMessage(() => i2i(1), Error, signatureMismatch);
assertEq(i2i(2, 100), 101);
assertEq(i2i(3, 100), 102);
assertEq(i2i(4, 100), 103);
assertEq(i2i(5, 100), 104);

assertErrorMessage(() => i2v(0), Error, signatureMismatch);
assertErrorMessage(() => i2v(1), Error, signatureMismatch);
assertErrorMessage(() => i2v(2), Error, signatureMismatch);
assertErrorMessage(() => i2v(3), Error, signatureMismatch);
assertErrorMessage(() => i2v(4), Error, signatureMismatch);
assertErrorMessage(() => i2v(5), Error, signatureMismatch);

{
    enableSPSProfiling();

    var stack;
    wasmFullPass(
        `(module
            (type $v2v (func))
            (import $foo "" "f")
            (func $a (call $foo))
            (func $b (result i32) (i32.const 0))
            (table anyfunc (elem $a $b))
            (func $bar (call_indirect $v2v (i32.const 0)))
            (export "run" $bar)
        )`,
        undefined,
        {"":{f:() => { stack = new Error().stack }}}
    );

    disableSPSProfiling();

    var inner = stack.indexOf("wasm-function[1]");
    var outer = stack.indexOf("wasm-function[3]");
    assertEq(inner === -1, false);
    assertEq(outer === -1, false);
    assertEq(inner < outer, true);
}

for (bad of [6, 7, 100, Math.pow(2,31)-1, Math.pow(2,31), Math.pow(2,31)+1, Math.pow(2,32)-2, Math.pow(2,32)-1]) {
    assertThrowsInstanceOf(() => v2i(bad), WebAssembly.RuntimeError);
    assertThrowsInstanceOf(() => i2i(bad, 0), WebAssembly.RuntimeError);
    assertThrowsInstanceOf(() => i2v(bad, 0), WebAssembly.RuntimeError);
}

wasmValidateText('(module (func $foo (nop)) (func (call $foo)))');
wasmValidateText('(module (func (call $foo)) (func $foo (nop)))');
wasmValidateText('(module (import $bar "" "a") (func (call $bar)) (func $foo (nop)))');

// ----------------------------------------------------------------------------
// select

wasmFailValidateText('(module (func (select (i32.const 0) (i32.const 0) (f32.const 0))))', mismatchError("f32", "i32"));

wasmFailValidateText('(module (func (select (i32.const 0) (f32.const 0) (i32.const 0))) (export "" 0))', /select operand types must match/);
wasmFailValidateText('(module (func (select (block ) (i32.const 0) (i32.const 0))) (export "" 0))', /popping value from empty stack/);
assertEq(wasmEvalText('(module (func (select (return) (i32.const 0) (i32.const 0))) (export "" 0))').exports[""](), undefined);
assertEq(wasmEvalText('(module (func (i32.add (i32.const 0) (select (return) (i32.const 0) (i32.const 0)))) (export "" 0))').exports[""](), undefined);
wasmFailValidateText('(module (func (select (if i32 (i32.const 1) (i32.const 0) (f32.const 0)) (i32.const 0) (i32.const 0))) (export "" 0))', mismatchError("f32", "i32"));
wasmFailValidateText('(module (func) (func (select (call 0) (call 0) (i32.const 0))) (export "" 0))', /popping value from empty stack/);

(function testSideEffects() {

var numT = 0;
var numF = 0;

var imports = {"": {
    ifTrue: () => 1 + numT++,
    ifFalse: () => -1 + numF++,
}}

// Test that side-effects are applied on both branches.
var f = wasmEvalText(`
(module
 (import "" "ifTrue" (result i32))
 (import "" "ifFalse" (result i32))
 (func (result i32) (param i32)
  (select
   (call 0)
   (call 1)
   (get_local 0)
  )
 )
 (export "" 2)
)
`, imports).exports[""];

assertEq(f(-1), numT);
assertEq(numT, 1);
assertEq(numF, 1);

assertEq(f(0), numF - 2);
assertEq(numT, 2);
assertEq(numF, 2);

assertEq(f(1), numT);
assertEq(numT, 3);
assertEq(numF, 3);

assertEq(f(0), numF - 2);
assertEq(numT, 4);
assertEq(numF, 4);

assertEq(f(0), numF - 2);
assertEq(numT, 5);
assertEq(numF, 5);

assertEq(f(1), numT);
assertEq(numT, 6);
assertEq(numF, 6);

})();

function testSelect(type, trueVal, falseVal) {

    var trueJS = jsify(trueVal);
    var falseJS = jsify(falseVal);

    // Always true condition
    var alwaysTrue = wasmEvalText(`
    (module
     (func (result ${type}) (param i32)
      (select
       (${type}.const ${trueVal})
       (${type}.const ${falseVal})
       (i32.const 1)
      )
     )
     (export "" 0)
    )
    `, imports).exports[""];

    assertEq(alwaysTrue(0), trueJS);
    assertEq(alwaysTrue(1), trueJS);
    assertEq(alwaysTrue(-1), trueJS);

    // Always false condition
    var alwaysFalse = wasmEvalText(`
    (module
     (func (result ${type}) (param i32)
      (select
       (${type}.const ${trueVal})
       (${type}.const ${falseVal})
       (i32.const 0)
      )
     )
     (export "" 0)
    )
    `, imports).exports[""];

    assertEq(alwaysFalse(0), falseJS);
    assertEq(alwaysFalse(1), falseJS);
    assertEq(alwaysFalse(-1), falseJS);

    // Variable condition
    var f = wasmEvalText(`
    (module
     (func (result ${type}) (param i32)
      (select
       (${type}.const ${trueVal})
       (${type}.const ${falseVal})
       (get_local 0)
      )
     )
     (export "" 0)
    )
    `, imports).exports[""];

    assertEq(f(0), falseJS);
    assertEq(f(1), trueJS);
    assertEq(f(-1), trueJS);

    wasmFullPass(`
    (module
     (func (result ${type}) (param i32)
      (select
       (${type}.const ${trueVal})
       (${type}.const ${falseVal})
       (get_local 0)
      )
     )
     (export "run" 0)
    )`,
    trueJS,
    imports,
    1);
}

testSelect('i32', 13, 37);
testSelect('i32', Math.pow(2, 31) - 1, -Math.pow(2, 31));

testSelect('f32', Math.fround(13.37), Math.fround(19.89));
testSelect('f32', 'infinity', '-0');
testSelect('f32', 'nan', Math.pow(2, -31));

testSelect('f64', 13.37, 19.89);
testSelect('f64', 'infinity', '-0');
testSelect('f64', 'nan', Math.pow(2, -31));

{
    setJitCompilerOption('wasm.test-mode', 1);

    var f = wasmEvalText(`
    (module
     (func (result i64) (param i32)
      (select
       (i64.const 0xc0010ff08badf00d)
       (i64.const 0x12345678deadc0de)
       (get_local 0)
      )
     )
     (export "" 0)
    )`, imports).exports[""];

    assertEqI64(f(0),  { low: 0xdeadc0de, high: 0x12345678});
    assertEqI64(f(1),  { low: 0x8badf00d, high: 0xc0010ff0});
    assertEqI64(f(-1), { low: 0x8badf00d, high: 0xc0010ff0});

    setJitCompilerOption('wasm.test-mode', 0);
}
