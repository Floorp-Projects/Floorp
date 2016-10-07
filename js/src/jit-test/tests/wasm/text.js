// |jit-test| test-also-wasm-baseline
load(libdir + "wasm.js");

var parsingError = /parsing wasm text at/;

assertErrorMessage(() => wasmEvalText(''), SyntaxError, parsingError);
assertErrorMessage(() => wasmEvalText('('), SyntaxError, parsingError);
assertErrorMessage(() => wasmEvalText('(m'), SyntaxError, parsingError);
assertErrorMessage(() => wasmEvalText('(module'), SyntaxError, parsingError);
assertErrorMessage(() => wasmEvalText('(moduler'), SyntaxError, parsingError);
assertErrorMessage(() => wasmEvalText('(module (func) (export "a'), SyntaxError, parsingError);
assertErrorMessage(() => wasmEvalText('(module (func (local $a i32) (param $b f32)))'), SyntaxError, parsingError);

assertErrorMessage(() => wasmEvalText('(module (func $a) (func) (export "a" $a) (export "b" $b))'), SyntaxError, /function label '\$b' not found/);
assertErrorMessage(() => wasmEvalText('(module (import $foo "a" "b") (import $foo "a" "b"))'), SyntaxError, /duplicate import/);
assertErrorMessage(() => wasmEvalText('(module (func $foo) (func $foo))'), SyntaxError, /duplicate function/);
assertErrorMessage(() => wasmEvalText('(module (func (param $a i32) (local $a i32)))'), SyntaxError, /duplicate var/);
assertErrorMessage(() => wasmEvalText('(module (func (get_local $a)))'), SyntaxError, /local label '\$a' not found/);
assertErrorMessage(() => wasmEvalText('(module (type $a (func)) (type $a (func (param i32))))'), SyntaxError, /duplicate signature/);
assertErrorMessage(() => wasmEvalText('(module (import "a" "") (func (call_import $abc)))'), SyntaxError, /function label '\$abc' not found/);
assertErrorMessage(() => wasmEvalText('(module (type $a (func)) (func (type $b) (i32.const 13)))'), SyntaxError, /signature label '\$b' not found/);
assertErrorMessage(() => wasmEvalText('(module (type $a (func)) (func (call_indirect $c (i32.const 0) (get_local 0))))'), SyntaxError, /signature label '\$c' not found/);
assertErrorMessage(() => wasmEvalText('(module (func (br $a)))'), SyntaxError, /branch target label '\$a' not found/);
assertErrorMessage(() => wasmEvalText('(module (func (block $a ) (br $a)))'), SyntaxError, /branch target label '\$a' not found/);

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
wasmEvalText('(module (table $t 1 10 anyfunc))');
wasmEvalText('(module (table $t 1 anyfunc))');
wasmEvalText('(module (table 0 anyfunc))');

assertErrorMessage(() => wasmEvalText('(module (table $t anyfunc))'), SyntaxError, parsingError);
wasmEvalText('(module (table $t anyfunc (elem)))');
wasmEvalText('(module (func) (table $t anyfunc (elem 0 0 0)))');

const { Table } = WebAssembly;
const table = new Table({initial:1, element:"anyfunc"});
assertErrorMessage(() => wasmEvalText('(module (table $t (import) 1 anyfunc))'), SyntaxError, parsingError);
assertErrorMessage(() => wasmEvalText('(module (table $t (import "mod") 1 anyfunc))'), SyntaxError, parsingError);
assertErrorMessage(() => wasmEvalText('(module (table $t (import "mod" "field") 1 anyfunc (elem 1 2 3)))'), SyntaxError, parsingError);
wasmEvalText('(module (table $t (import "mod" "field") 1 anyfunc))', {mod: {field: table}});

assertErrorMessage(() => wasmEvalText('(module (table $t (export "mod") 1))'), SyntaxError, parsingError);
assertErrorMessage(() => wasmEvalText('(module (table $t (export "mod") anyfunc))'), SyntaxError, parsingError);
assertErrorMessage(() => wasmEvalText('(module (table $t (export "mod") anyfunc 1 2 3))'), SyntaxError, parsingError);
assertEq(wasmEvalText('(module (table $t (export "tbl") anyfunc (elem)))').exports.tbl instanceof Table, true);
assertEq(wasmEvalText('(module (func) (table $t (export "tbl") anyfunc (elem 0 0 0)))').exports.tbl instanceof Table, true);

// Note: the s-expression text format is temporary, this file is mostly just to
// hold basic error smoke tests.
