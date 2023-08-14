const { LinkError } = WebAssembly;

// ----------------------------------------------------------------------------
// exports

var o = wasmEvalText('(module)').exports;
assertEq(Object.getOwnPropertyNames(o).length, 0);

var o = wasmEvalText('(module (func))').exports;
assertEq(Object.getOwnPropertyNames(o).length, 0);

var o = wasmEvalText('(module (func) (export "a" (func 0)))').exports;
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

wasmValidateText('(module (func) (func) (export "a" (func 0)))');
wasmValidateText('(module (func) (func) (export "a" (func 1)))');
wasmValidateText('(module (func $a) (func $b) (export "a" (func $a)) (export "b" (func $b)))');
wasmValidateText('(module (func $a) (func $b) (export "a" (func $a)) (export "b" (func $b)))');

wasmFailValidateText('(module (func) (export "a" (func 1)))', /exported function index out of bounds/);
wasmFailValidateText('(module (func) (func) (export "a" (func 2)))', /exported function index out of bounds/);

var o = wasmEvalText('(module (func) (export "a" (func 0)) (export "b" (func 0)))').exports;
assertEq(Object.getOwnPropertyNames(o).sort().toString(), "a,b");
assertEq(o.a.name, "0");
assertEq(o.b.name, "0");
assertEq(o.a === o.b, true);

var o = wasmEvalText('(module (func) (func) (export "a" (func 0)) (export "b" (func 1)))').exports;
assertEq(Object.getOwnPropertyNames(o).sort().toString(), "a,b");
assertEq(o.a.name, "0");
assertEq(o.b.name, "1");
assertEq(o.a === o.b, false);

var o = wasmEvalText('(module (func (result i32) (i32.const 1)) (func (result i32) (i32.const 2)) (export "a" (func 0)) (export "b" (func 1)))').exports;
assertEq(o.a(), 1);
assertEq(o.b(), 2);
var o = wasmEvalText('(module (func (result i32) (i32.const 1)) (func (result i32) (i32.const 2)) (export "a" (func 1)) (export "b" (func 0)))').exports;
assertEq(o.a(), 2);
assertEq(o.b(), 1);

wasmFailValidateText('(module (func) (export "a" (func 0)) (export "a" (func 0)))', /duplicate export/);
wasmFailValidateText('(module (func) (func) (export "a" (func 0)) (export "a" (func 1)))', /duplicate export/);

// ----------------------------------------------------------------------------
// signatures

wasmFailValidateText('(module (func (result i32)))', emptyStackError);
wasmFailValidateText('(module (func (result i32) (nop)))', emptyStackError);

wasmValidateText('(module (func (nop)))');
wasmValidateText('(module (func (result i32) (i32.const 42)))');
wasmValidateText('(module (func (param i32)))');
wasmValidateText('(module (func (param i32) (result i32) (i32.const 42)))');
wasmValidateText('(module (func (param i32) (result i32) (i32.const 42)))');
wasmValidateText('(module (func (param f32)))');
wasmValidateText('(module (func (param f64)))');

wasmFullPassI64('(module (func $run (result i64) (i64.const 123)))', 123);
wasmFullPassI64('(module (func $run (param i64) (result i64) (local.get 0)))',
                '0x123400007fffffff',
                {},
                '(i64.const 0x123400007fffffff)');
wasmFullPassI64('(module (func $run (param i64) (result i64) (i64.add (local.get 0) (i64.const 1))))',
                '0x1234000100000000',
                {},
                '(i64.const 0x12340000ffffffff)');

// ----------------------------------------------------------------------------
// imports

const noImportObj = "second argument must be an object";

assertErrorMessage(() => wasmEvalText('(module (import "a" "b" (func)))', 1), TypeError, noImportObj);
assertErrorMessage(() => wasmEvalText('(module (import "a" "b" (func)))', null), TypeError, noImportObj);

const notObject = /import object field '\w*' is not an Object/;
const notFunction = /import object field '\w*' is not a Function/;

var code = '(module (import "a" "b" (func)))';
assertErrorMessage(() => wasmEvalText(code), TypeError, noImportObj);
assertErrorMessage(() => wasmEvalText(code, {}), TypeError, notObject);
assertErrorMessage(() => wasmEvalText(code, {a:1}), TypeError, notObject);
assertErrorMessage(() => wasmEvalText(code, {a:{}}), LinkError, notFunction);
assertErrorMessage(() => wasmEvalText(code, {a:{b:1}}), LinkError, notFunction);
wasmEvalText(code, {a:{b:()=>{}}});

var code = '(module (import "" "b" (func)))';
wasmEvalText(code, {"":{b:()=>{}}});

var code = '(module (import "a" "" (func)))';
assertErrorMessage(() => wasmEvalText(code), TypeError, noImportObj);
assertErrorMessage(() => wasmEvalText(code, {}), TypeError, notObject);
assertErrorMessage(() => wasmEvalText(code, {a:1}), TypeError, notObject);
wasmEvalText(code, {a:{"":()=>{}}});

var code = '(module (import "a" "" (func)) (import "b" "c" (func)) (import "c" "" (func)))';
assertErrorMessage(() => wasmEvalText(code, {a:()=>{}, b:{c:()=>{}}, c:{}}), LinkError, notFunction);
wasmEvalText(code, {a:{"":()=>{}}, b:{c:()=>{}}, c:{"":()=>{}}});

wasmEvalText('(module (import "a" "" (func (result i32))))', {a:{"":()=>{}}});
wasmEvalText('(module (import "a" "" (func (result f32))))', {a:{"":()=>{}}});
wasmEvalText('(module (import "a" "" (func (result f64))))', {a:{"":()=>{}}});
wasmEvalText('(module (import "a" "" (func $foo (result f64))))', {a:{"":()=>{}}});

// ----------------------------------------------------------------------------
// memory

wasmValidateText('(module (memory 0))');
wasmValidateText('(module (memory 1))');

var buf = wasmEvalText('(module (memory 1) (export "memory" (memory 0)))').exports.memory.buffer;
assertEq(buf instanceof ArrayBuffer, true);
assertEq(buf.byteLength, 65536);

var obj = wasmEvalText('(module (memory 1) (func (result i32) (i32.const 42)) (func (nop)) (export "memory" (memory 0)) (export "b" (func 0)) (export "c" (func 1)))').exports;
assertEq(obj.memory.buffer instanceof ArrayBuffer, true);
assertEq(obj.b instanceof Function, true);
assertEq(obj.c instanceof Function, true);
assertEq(obj.memory.buffer.byteLength, 65536);
assertEq(obj.b(), 42);
assertEq(obj.c(), undefined);

var buf = wasmEvalText('(module (memory 1) (data (i32.const 0) "") (export "memory" (memory 0)))').exports.memory.buffer;
assertEq(new Uint8Array(buf)[0], 0);

var buf = wasmEvalText('(module (memory 1) (data (i32.const 65536) "") (export "memory" (memory 0)))').exports.memory.buffer;
assertEq(new Uint8Array(buf)[0], 0);

var buf = wasmEvalText('(module (memory 1) (data (i32.const 0) "a") (export "memory" (memory 0)))').exports.memory.buffer;
assertEq(new Uint8Array(buf)[0], 'a'.charCodeAt(0));

var buf = wasmEvalText('(module (memory 1) (data (i32.const 0) "a") (data (i32.const 2) "b") (export "memory" (memory 0)))').exports.memory.buffer;
assertEq(new Uint8Array(buf)[0], 'a'.charCodeAt(0));
assertEq(new Uint8Array(buf)[1], 0);
assertEq(new Uint8Array(buf)[2], 'b'.charCodeAt(0));

var buf = wasmEvalText('(module (memory 1) (data (i32.const 65535) "c") (export "memory" (memory 0)))').exports.memory.buffer;
assertEq(new Uint8Array(buf)[0], 0);
assertEq(new Uint8Array(buf)[65535], 'c'.charCodeAt(0));

// ----------------------------------------------------------------------------
// locals

assertEq(wasmEvalText('(module (func (param i32) (result i32) (local.get 0)) (export "" (func 0)))').exports[""](), 0);
assertEq(wasmEvalText('(module (func (param i32) (result i32) (local.get 0)) (export "" (func 0)))').exports[""](42), 42);
assertEq(wasmEvalText('(module (func (param i32) (param i32) (result i32) (local.get 0)) (export "" (func 0)))').exports[""](42, 43), 42);
assertEq(wasmEvalText('(module (func (param i32) (param i32) (result i32) (local.get 1)) (export "" (func 0)))').exports[""](42, 43), 43);

wasmFailValidateText('(module (func (local.get 0)))', /(local.get index out of range)|(local index out of bounds)/);
wasmFailValidateText('(module (func (result f32) (local i32) (local.get 0)))', mismatchError("i32", "f32"));
wasmFailValidateText('(module (func (result i32) (local f32) (local.get 0)))', mismatchError("f32", "i32"));
wasmFailValidateText('(module (func (param i32) (result f32) (local f32) (local.get 0)))', mismatchError("i32", "f32"));
wasmFailValidateText('(module (func (param i32) (result i32) (local f32) (local.get 1)))', mismatchError("f32", "i32"));

wasmValidateText('(module (func (local i32)))');
wasmValidateText('(module (func (local i32) (local f32)))');

wasmFullPass('(module (func (result i32) (local i32) (local.get 0)) (export "run" (func 0)))', 0);
wasmFullPass('(module (func (param i32) (result i32) (local f32) (local.get 0)) (export "run" (func 0)))', 0);
wasmFullPass('(module (func (param i32) (result f32) (local f32) (local.get 1)) (export "run" (func 0)))', 0);

wasmFailValidateText('(module (func (local.set 0 (i32.const 0))))', /(local.set index out of range)|(local index out of bounds)/);
wasmFailValidateText('(module (func (local f32) (local.set 0 (i32.const 0))))', mismatchError("i32", "f32"));
wasmFailValidateText('(module (func (local f32) (local.set 0 (nop))))', emptyStackError);
wasmFailValidateText('(module (func (local i32) (local f32) (local.set 0 (local.get 1))))', mismatchError("f32", "i32"));
wasmFailValidateText('(module (func (local i32) (local f32) (local.set 1 (local.get 0))))', mismatchError("i32", "f32"));

wasmValidateText('(module (func (local i32) (local.set 0 (i32.const 0))))');
wasmValidateText('(module (func (local i32) (local f32) (local.set 0 (local.get 0))))');
wasmValidateText('(module (func (local i32) (local f32) (local.set 1 (local.get 1))))');

wasmFullPass('(module (func (result i32) (local i32) (local.tee 0 (i32.const 42))) (export "run" (func 0)))', 42);
wasmFullPass('(module (func (result i32) (local i32) (local.tee 0 (local.get 0))) (export "run" (func 0)))', 0);

wasmFullPass('(module (func (param $a i32) (result i32) (local.get $a)) (export "run" (func 0)))', 0);
wasmFullPass('(module (func (param $a i32) (result i32) (local $b i32) (block (result i32) (local.set $b (local.get $a)) (local.get $b))) (export "run" (func 0)))', 42, {}, 42);

wasmValidateText('(module (func (local i32) (local $a f32) (local.set 0 (i32.const 1)) (local.set $a (f32.const nan))))');

// ----------------------------------------------------------------------------
// blocks

wasmFullPass('(module (func (block )) (export "run" (func 0)))', undefined);

wasmFailValidateText('(module (func (result i32) (block )))', emptyStackError);
wasmFailValidateText('(module (func (result i32) (block (block ))))', emptyStackError);
wasmFailValidateText('(module (func (local i32) (local.set 0 (block ))))', emptyStackError);

wasmFullPass('(module (func (block (block ))) (export "run" (func 0)))', undefined);
wasmFullPass('(module (func (result i32) (block (result i32) (i32.const 42))) (export "run" (func 0)))', 42);
wasmFullPass('(module (func (result i32) (block (result i32) (block (result i32) (i32.const 42)))) (export "run" (func 0)))', 42);
wasmFailValidateText('(module (func (result f32) (block (result i32) (i32.const 0))))', mismatchError("i32", "f32"));

wasmFullPass('(module (func (result i32) (block (result i32) (drop (i32.const 13)) (block (result i32) (i32.const 42)))) (export "run" (func 0)))', 42);
wasmFailValidateText('(module (func (param f32) (result f32) (block (result i32) (drop (local.get 0)) (i32.const 0))))', mismatchError("i32", "f32"));

wasmFullPass('(module (func (result i32) (local i32) (local.set 0 (i32.const 42)) (local.get 0)) (export "run" (func 0)))', 42);

// ----------------------------------------------------------------------------
// calls

wasmFailValidateText('(module (func (nop)) (func (call 0 (i32.const 0))))', unusedValuesError);

wasmFailValidateText('(module (func (param i32) (nop)) (func (call 0)))', emptyStackError);
wasmFailValidateText('(module (func (param f32) (nop)) (func (call 0 (i32.const 0))))', mismatchError("i32", "f32"));
wasmFailValidateText('(module (func (nop)) (func (call 3)))', /(callee index out of range)|(function index out of bounds)/);

wasmValidateText('(module (func (nop)) (func (call 0)))');
wasmValidateText('(module (func (param i32) (nop)) (func (call 0 (i32.const 0))))');

wasmFullPass('(module (func (result i32) (i32.const 42)) (func (result i32) (call 0)) (export "run" (func 1)))', 42);
assertThrowsInstanceOf(() => wasmEvalText('(module (func (call 0)) (export "" (func 0)))').exports[""](), InternalError);
assertThrowsInstanceOf(() => wasmEvalText('(module (func (call 1)) (func (call 0)) (export "" (func 0)))').exports[""](), InternalError);

wasmValidateText('(module (func (param i32 f32)) (func (call 0 (i32.const 0) (f32.const nan))))');
wasmFailValidateText('(module (func (param i32 f32)) (func (call 0 (i32.const 0) (i32.const 0))))', mismatchError("i32", "f32"));

wasmFailValidateText('(module (import "a" "" (func)) (func (call 0 (i32.const 0))))', unusedValuesError);
wasmFailValidateText('(module (import "a" "" (func (param i32))) (func (call 0)))', emptyStackError);
wasmFailValidateText('(module (import "a" "" (func (param f32))) (func (call 0 (i32.const 0))))', mismatchError("i32", "f32"));

assertErrorMessage(() => wasmEvalText('(module (import "a" "" (func)) (func (call 1)))'), TypeError, noImportObj);
wasmEvalText('(module (import "" "a" (func)) (func (call 0)))', {"":{a:()=>{}}});
wasmEvalText('(module (import "" "a" (func (param i32))) (func (call 0 (i32.const 0))))', {"":{a:()=>{}}});

function checkF32CallImport(v) {
    wasmFullPass('(module (import "" "a" (func (result f32))) (func (result f32) (call 0)) (export "run" (func 1)))',
                 Math.fround(v),
                 {"":{a:()=>{ return v; }}});
    wasmFullPass('(module (import "" "a" (func (param f32))) (func (param f32) (call 0 (local.get 0))) (export "run" (func 1)))',
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
var f = wasmEvalText('(module (import "" "inc" (func)) (func (call 0)) (export "" (func 1)))', {"":{inc:()=>counter++}}).exports[""];
var g = wasmEvalText('(module (import "" "f" (func)) (func (block (call 0) (call 0))) (export "" (func 1)))', {"":{f}}).exports[""];
f();
assertEq(counter, 1);
g();
assertEq(counter, 3);

var f = wasmEvalText('(module (import "" "callf" (func)) (func (call 0)) (export "" (func 1)))', {"":{callf:()=>f()}}).exports[""];
assertThrowsInstanceOf(() => f(), InternalError);

var f = wasmEvalText('(module (import "" "callg" (func)) (func (call 0)) (export "" (func 1)))', {"":{callg:()=>g()}}).exports[""];
var g = wasmEvalText('(module (import "" "callf" (func)) (func (call 0)) (export "" (func 1)))', {"":{callf:()=>f()}}).exports[""];
assertThrowsInstanceOf(() => f(), InternalError);

var code = '(module (import "" "one" (func (result i32))) (import "" "two" (func (result i32))) (func (result i32) (i32.const 3)) (func (result i32) (i32.const 4)) (func (result i32) BODY) (export "run" (func 4)))';
var imports = {"":{one:()=>1, two:()=>2}};
wasmFullPass(code.replace('BODY', '(call 0)'), 1, imports);
wasmFullPass(code.replace('BODY', '(call 1)'), 2, imports);
wasmFullPass(code.replace('BODY', '(call 2)'), 3, imports);
wasmFullPass(code.replace('BODY', '(call 3)'), 4, imports);

wasmFullPass(`(module (import "" "evalcx" (func (param i32) (result i32))) (func (result i32) (call 0 (i32.const 0))) (export "run" (func 1)))`, 0, {"":{evalcx}});

if (typeof evaluate === 'function')
    evaluate(`new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary('(module)'))) `, { fileName: null });

wasmFailValidateText(`(module (type $t (func)) (func (call_indirect (type $t) (i32.const 0))))`, /(can't call_indirect without a table)|(unknown table)/);

var {v2i, i2i, i2v} = wasmEvalText(`(module
    (type (func (result i32)))
    (type (func (param i32) (result i32)))
    (type (func (param i32)))
    (func (type 0) (i32.const 13))
    (func (type 0) (i32.const 42))
    (func (type 1) (i32.add (local.get 0) (i32.const 1)))
    (func (type 1) (i32.add (local.get 0) (i32.const 2)))
    (func (type 1) (i32.add (local.get 0) (i32.const 3)))
    (func (type 1) (i32.add (local.get 0) (i32.const 4)))
    (table funcref (elem 0 1 2 3 4 5))
    (func (param i32) (result i32) (call_indirect (type 0) (local.get 0)))
    (func (param i32) (param i32) (result i32) (call_indirect (type 1) (local.get 1) (local.get 0)))
    (func (param i32) (call_indirect (type 2) (i32.const 0) (local.get 0)))
    (export "v2i" (func 6))
    (export "i2i" (func 7))
    (export "i2v" (func 8))
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
    enableGeckoProfiling();

    var stack;
    wasmFullPass(
        `(module
            (type $v2v (func))
            (import "" "f" (func $foo))
            (func $a (call $foo))
            (func $b (result i32) (i32.const 0))
            (table funcref (elem $a $b))
            (func $bar (call_indirect (type $v2v) (i32.const 0)))
            (export "run" (func $bar))
        )`,
        undefined,
        {"":{f:() => { stack = new Error().stack }}}
    );

    disableGeckoProfiling();

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
wasmValidateText('(module (import "" "a" (func $bar)) (func (call $bar)) (func $foo (nop)))');

var exp = wasmEvalText(`(module (func (export "f")))`).exports;
assertEq(Object.isFrozen(exp), true);
