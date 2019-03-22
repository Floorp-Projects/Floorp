var parsingError = /parsing wasm text at/;

assertErrorMessage(() => wasmEvalText(''), SyntaxError, parsingError);
assertErrorMessage(() => wasmEvalText('('), SyntaxError, parsingError);
assertErrorMessage(() => wasmEvalText('(m'), SyntaxError, parsingError);
assertErrorMessage(() => wasmEvalText('(module'), SyntaxError, parsingError);
assertErrorMessage(() => wasmEvalText('(moduler'), SyntaxError, parsingError);
assertErrorMessage(() => wasmEvalText('(module (func) (export "a'), SyntaxError, parsingError);
assertErrorMessage(() => wasmEvalText('(module (func (local $a i32) (param $b f32)))'), SyntaxError, parsingError);

assertErrorMessage(() => wasmEvalText('(module (func $a) (func) (export "a" $a) (export "b" $b))'), SyntaxError, /Function label '\$b' not found/);
assertErrorMessage(() => wasmEvalText('(module (import $foo "a" "b") (import $foo "a" "b"))'), SyntaxError, /duplicate import/);
assertErrorMessage(() => wasmEvalText('(module (func $foo) (func $foo))'), SyntaxError, /duplicate function/);
assertErrorMessage(() => wasmEvalText('(module (func (param $a i32) (local $a i32)))'), SyntaxError, /duplicate var/);
assertErrorMessage(() => wasmEvalText('(module (func (local.get $a)))'), SyntaxError, /Local label '\$a' not found/);
assertErrorMessage(() => wasmEvalText('(module (type $a (func)) (type $a (func (param i32))))'), SyntaxError, /duplicate signature/);
assertErrorMessage(() => wasmEvalText('(module (import "a" "") (func (call $abc)))'), SyntaxError, /Function label '\$abc' not found/);
assertErrorMessage(() => wasmEvalText('(module (type $a (func)) (func (type $b) (i32.const 13)))'), SyntaxError, /Signature label '\$b' not found/);
assertErrorMessage(() => wasmEvalText('(module (type $a (func)) (func (call_indirect $c (i32.const 0) (local.get 0))))'), SyntaxError, /Signature label '\$c' not found/);
assertErrorMessage(() => wasmEvalText('(module (func (br $a)))'), SyntaxError, /branch target label '\$a' not found/);
assertErrorMessage(() => wasmEvalText('(module (func (block $a ) (br $a)))'), SyntaxError, /branch target label '\$a' not found/);

assertErrorMessage(() => wasmEvalText(`(module (func (call ${0xffffffff})))`), SyntaxError, parsingError);
assertErrorMessage(() => wasmEvalText(`(module (export "func" ${0xffffffff}))`), SyntaxError, parsingError);

wasmEvalText('(module (func (param $a i32)))');
wasmEvalText('(module (func (param i32)))');
wasmEvalText('(module (func (param i32 i32 f32 f64 i32)))');
assertErrorMessage(() => wasmEvalText('(module (func (param $a)))'), SyntaxError, parsingError);
assertErrorMessage(() => wasmEvalText('(module (func (param $a i32 i32)))'), SyntaxError, parsingError);

wasmEvalText('(module (func (local $a i32)))');
wasmEvalText('(module (func (local i32)))');
wasmEvalText('(module (func (local i32 i32 f32 f64 i32)))');
assertErrorMessage(() => wasmEvalText('(module (func (local $a)))'), SyntaxError, parsingError);
assertErrorMessage(() => wasmEvalText('(module (func (local $a i32 i32)))'), SyntaxError, parsingError);

// Table
assertErrorMessage(() => wasmEvalText('(module (table (local $a)))'), SyntaxError, parsingError);
assertErrorMessage(() => wasmEvalText('(module (table $t))'), SyntaxError, parsingError);
assertErrorMessage(() => wasmEvalText('(module (table $t 1))'), SyntaxError, parsingError);
assertErrorMessage(() => wasmEvalText('(module (table $t 1 10))'), SyntaxError, parsingError);
wasmEvalText('(module (table $t 1 10 funcref))');
wasmEvalText('(module (table $t 1 funcref))');
wasmEvalText('(module (table 0 funcref))');

assertErrorMessage(() => wasmEvalText('(module (table $t funcref))'), SyntaxError, parsingError);
wasmEvalText('(module (table $t funcref (elem)))');
wasmEvalText('(module (func) (table $t funcref (elem 0 0 0)))');

const { Table } = WebAssembly;
const table = new Table({initial:1, element:"funcref"});
assertErrorMessage(() => wasmEvalText('(module (table $t (import) 1 funcref))'), SyntaxError, parsingError);
assertErrorMessage(() => wasmEvalText('(module (table $t (import "mod") 1 funcref))'), SyntaxError, parsingError);
assertErrorMessage(() => wasmEvalText('(module (table $t (import "mod" "field") 1 funcref (elem 1 2 3)))'), SyntaxError, parsingError);
wasmEvalText('(module (table $t (import "mod" "field") 1 funcref))', {mod: {field: table}});

assertErrorMessage(() => wasmEvalText('(module (table $t (export "mod") 1))'), SyntaxError, parsingError);
assertErrorMessage(() => wasmEvalText('(module (table $t (export "mod") funcref))'), SyntaxError, parsingError);
assertErrorMessage(() => wasmEvalText('(module (table $t (export "mod") funcref 1 2 3))'), SyntaxError, parsingError);
assertEq(wasmEvalText('(module (table $t (export "tbl") funcref (elem)))').exports.tbl instanceof Table, true);
assertEq(wasmEvalText('(module (func) (table $t (export "tbl") funcref (elem 0 0 0)))').exports.tbl instanceof Table, true);

// Functions.
assertErrorMessage(() => wasmEvalText('(module (func $t import))'), SyntaxError, parsingError);
assertErrorMessage(() => wasmEvalText('(module (func $t (import)))'), SyntaxError, parsingError);
assertErrorMessage(() => wasmEvalText('(module (func $t (import "mod")))'), SyntaxError, parsingError);
assertErrorMessage(() => wasmEvalText('(module (func $t (import "mod" "func" (local i32))))'), SyntaxError, parsingError);

const func = i => 42 + i;
wasmEvalText('(module (func $t (import "mod" "func")))', { mod: {func} });
wasmEvalText('(module (func $t (import "mod" "func")(param i32)))', { mod: {func} });
wasmEvalText('(module (func $t (import "mod" "func")(result i32)))', { mod: {func} });
wasmEvalText('(module (func $t (import "mod" "func")(param i32) (result i32)))', { mod: {func} });
wasmEvalText('(module (func $t (import "mod" "func")(result i32) (param i32)))', { mod: {func} });

assertErrorMessage(() => wasmEvalText('(module (func $t (import "mod" "func") (type)))', { mod: {func} }), SyntaxError, parsingError);
wasmEvalText('(module (type $t (func)) (func $t (import "mod" "func") (type $t)))', { mod: {func} });

assertErrorMessage(() => wasmEvalText('(module (func $t (export))))'), SyntaxError, parsingError);
wasmEvalText('(module (func (export "f")))');
wasmEvalText('(module (func $f (export "f")))');
wasmEvalText('(module (func $f (export "f") (result i32) (param i32) (i32.add (local.get 0) (i32.const 42))))');

assertErrorMessage(() => wasmEvalText(`
    (module
        (type $tf (func (param i32) (result i32)))
        (func (import "mod" "a") (type $tf))
        (func (export "f1"))
        (func (import "mod" "b") (type $tf))
        (func (export "f2"))
    )
`), SyntaxError, /import after function definition/);

// Globals.
assertErrorMessage(() => wasmEvalText('(module (global $t (export)))'), SyntaxError, parsingError);
assertErrorMessage(() => wasmEvalText('(module (global $t (export "g")))'), SyntaxError, parsingError);
assertErrorMessage(() => wasmEvalText('(module (global $t (export "g") i32))'), SyntaxError, parsingError);
wasmEvalText('(module (global $t (export "g") i32 (i32.const 42)))');

assertErrorMessage(() => wasmEvalText('(module (global $t (import) i32))'), SyntaxError, parsingError);
assertErrorMessage(() => wasmEvalText('(module (global $t (import "mod") i32))'), SyntaxError, parsingError);
assertErrorMessage(() => wasmEvalText('(module (global $t (import "mod" "field")))'), SyntaxError, parsingError);
assertErrorMessage(() => wasmEvalText('(module (global $t (import "mod" "field")) i32 (i32.const 42))'), SyntaxError, parsingError);
wasmEvalText('(module (global $t (import "mod" "field") i32))', { mod: {field: 42} });

assertErrorMessage(() => wasmEvalText(`
    (module
        (global (import "mod" "a") i32)
        (global (export "f1") i32 (i32.const 42))
        (global (import "mod" "b") i32)
    )
`), SyntaxError, /import after global definition/);

// Memory.
assertErrorMessage(() => wasmEvalText('(module (memory (export)))'), SyntaxError, parsingError);
assertErrorMessage(() => wasmEvalText('(module (memory (export "g")))'), SyntaxError, parsingError);
wasmEvalText('(module (memory $t (export "g") 0))');

const mem = new WebAssembly.Memory({ initial: 1 });
assertErrorMessage(() => wasmEvalText('(module (memory $t (import) 1))'), SyntaxError, parsingError);
assertErrorMessage(() => wasmEvalText('(module (memory $t (import "mod") 1))'), SyntaxError, parsingError);
assertErrorMessage(() => wasmEvalText('(module (memory $t (import "mod" "field")))'), SyntaxError, parsingError);
wasmEvalText('(module (memory $t (import "mod" "field") 1))', { mod: {field: mem} });

// Note: the s-expression text format is temporary, this file is mostly just to
// hold basic error smoke tests.
