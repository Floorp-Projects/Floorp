// |jit-test| test-also-wasm-baseline
load(libdir + 'wasm.js');

const Module = WebAssembly.Module;
const Instance = WebAssembly.Instance;
const Memory = WebAssembly.Memory;
const Table = WebAssembly.Table;

const mem1Page = new Memory({initial:1});
const mem1PageMax1 = new Memory({initial:1, maximum: 1});
const mem2Page = new Memory({initial:2});
const mem2PageMax2 = new Memory({initial:2, maximum: 2});
const mem2PageMax3 = new Memory({initial:2, maximum: 3});
const mem2PageMax4 = new Memory({initial:2, maximum: 4});
const mem3Page = new Memory({initial:3});
const mem3PageMax3 = new Memory({initial:3, maximum: 3});
const mem4Page = new Memory({initial:4});
const mem4PageMax4 = new Memory({initial:4, maximum: 4});
const tab1Elem = new Table({initial:1, element:"anyfunc"});
const tab2Elem = new Table({initial:2, element:"anyfunc"});
const tab3Elem = new Table({initial:3, element:"anyfunc"});
const tab4Elem = new Table({initial:4, element:"anyfunc"});

assertErrorMessage(() => new Memory({initial:2, maximum:1}), RangeError, /bad Memory maximum size/);

const m1 = new Module(wasmTextToBinary('(module (import "foo" "bar") (import "baz" "quux"))'));
assertErrorMessage(() => new Instance(m1), TypeError, /second argument must be an object/);
assertErrorMessage(() => new Instance(m1, {foo:null}), TypeError, /import object field is not an Object/);
assertErrorMessage(() => new Instance(m1, {foo:{bar:{}}}), TypeError, /import object field is not a Function/);
assertErrorMessage(() => new Instance(m1, {foo:{bar:()=>{}}, baz:null}), TypeError, /import object field is not an Object/);
assertErrorMessage(() => new Instance(m1, {foo:{bar:()=>{}}, baz:{}}), TypeError, /import object field is not a Function/);
assertEq(new Instance(m1, {foo:{bar:()=>{}}, baz:{quux:()=>{}}}) instanceof Instance, true);

const m2 = new Module(wasmTextToBinary('(module (import "x" "y" (memory 2 3)))'));
assertErrorMessage(() => new Instance(m2), TypeError, /second argument must be an object/);
assertErrorMessage(() => new Instance(m2, {x:null}), TypeError, /import object field is not an Object/);
assertErrorMessage(() => new Instance(m2, {x:{y:{}}}), TypeError, /import object field is not a Memory/);
assertErrorMessage(() => new Instance(m2, {x:{y:mem1Page}}), TypeError, /imported Memory with incompatible size/);
assertErrorMessage(() => new Instance(m2, {x:{y:mem1PageMax1}}), TypeError, /imported Memory with incompatible size/);
assertErrorMessage(() => new Instance(m2, {x:{y:mem4Page}}), TypeError, /imported Memory with incompatible size/);
assertErrorMessage(() => new Instance(m2, {x:{y:mem4PageMax4}}), TypeError, /imported Memory with incompatible size/);
assertErrorMessage(() => new Instance(m2, {x:{y:mem2Page}}), TypeError, /imported Memory with incompatible maximum size/);
assertEq(new Instance(m2, {x:{y:mem2PageMax2}}) instanceof Instance, true);
assertErrorMessage(() => new Instance(m2, {x:{y:mem3Page}}), TypeError, /imported Memory with incompatible maximum size/);
assertEq(new Instance(m2, {x:{y:mem3PageMax3}}) instanceof Instance, true);
assertEq(new Instance(m2, {x:{y:mem2PageMax3}}) instanceof Instance, true);
assertErrorMessage(() => new Instance(m2, {x:{y:mem2PageMax4}}), TypeError, /imported Memory with incompatible maximum size/);

const m3 = new Module(wasmTextToBinary('(module (import "foo" "bar" (memory 1 1)) (import "baz" "quux"))'));
assertErrorMessage(() => new Instance(m3), TypeError, /second argument must be an object/);
assertErrorMessage(() => new Instance(m3, {foo:null}), TypeError, /import object field is not an Object/);
assertErrorMessage(() => new Instance(m3, {foo:{bar:{}}}), TypeError, /import object field is not a Memory/);
assertErrorMessage(() => new Instance(m3, {foo:{bar:mem1Page}, baz:null}), TypeError, /import object field is not an Object/);
assertErrorMessage(() => new Instance(m3, {foo:{bar:mem1Page}, baz:{quux:mem1Page}}), TypeError, /import object field is not a Function/);
assertErrorMessage(() => new Instance(m3, {foo:{bar:mem1Page}, baz:{quux:()=>{}}}), TypeError, /imported Memory with incompatible maximum size/);
assertEq(new Instance(m3, {foo:{bar:mem1PageMax1}, baz:{quux:()=>{}}}) instanceof Instance, true);

const m4 = new Module(wasmTextToBinary('(module (import "baz" "quux") (import "foo" "bar" (memory 1 1)))'));
assertErrorMessage(() => new Instance(m4), TypeError, /second argument must be an object/);
assertErrorMessage(() => new Instance(m4, {baz:null}), TypeError, /import object field is not an Object/);
assertErrorMessage(() => new Instance(m4, {baz:{quux:{}}}), TypeError, /import object field is not a Function/);
assertErrorMessage(() => new Instance(m4, {baz:{quux:()=>{}}, foo:null}), TypeError, /import object field is not an Object/);
assertErrorMessage(() => new Instance(m4, {baz:{quux:()=>{}}, foo:{bar:()=>{}}}), TypeError, /import object field is not a Memory/);
assertErrorMessage(() => new Instance(m4, {baz:{quux:()=>{}}, foo:{bar:mem1Page}}), TypeError, /imported Memory with incompatible maximum size/);
assertEq(new Instance(m3, {baz:{quux:()=>{}}, foo:{bar:mem1PageMax1}}) instanceof Instance, true);

const m5 = new Module(wasmTextToBinary('(module (import "a" "b" (memory 2)))'));
assertErrorMessage(() => new Instance(m5, {a:{b:mem1Page}}), TypeError, /imported Memory with incompatible size/);
assertEq(new Instance(m5, {a:{b:mem2Page}}) instanceof Instance, true);
assertEq(new Instance(m5, {a:{b:mem3Page}}) instanceof Instance, true);
assertEq(new Instance(m5, {a:{b:mem4Page}}) instanceof Instance, true);

const m6 = new Module(wasmTextToBinary('(module (import "a" "b" (table 2)))'));
assertErrorMessage(() => new Instance(m6, {a:{b:tab1Elem}}), TypeError, /imported Table with incompatible size/);
assertEq(new Instance(m6, {a:{b:tab2Elem}}) instanceof Instance, true);
assertEq(new Instance(m6, {a:{b:tab3Elem}}) instanceof Instance, true);
assertEq(new Instance(m6, {a:{b:tab4Elem}}) instanceof Instance, true);

const m7 = new Module(wasmTextToBinary('(module (import "a" "b" (table 2 3)))'));
assertErrorMessage(() => new Instance(m7, {a:{b:tab1Elem}}), TypeError, /imported Table with incompatible size/);
assertErrorMessage(() => new Instance(m7, {a:{b:tab2Elem}}), TypeError, /imported Table with incompatible maximum size/);
assertErrorMessage(() => new Instance(m7, {a:{b:tab3Elem}}), TypeError, /imported Table with incompatible maximum size/);
assertErrorMessage(() => new Instance(m7, {a:{b:tab4Elem}}), TypeError, /imported Table with incompatible size/);

wasmFailValidateText('(module (memory 2 1))', /maximum length 1 is less than initial length 2/);
wasmFailValidateText('(module (import "a" "b" (memory 2 1)))', /maximum length 1 is less than initial length 2/);
wasmFailValidateText('(module (table (resizable 2 1)))', /maximum length 1 is less than initial length 2/);
wasmFailValidateText('(module (import "a" "b" (table 2 1)))', /maximum length 1 is less than initial length 2/);

// Import wasm-wasm type mismatch

var e = wasmEvalText('(module (func $i2v (param i32)) (export "i2v" $i2v) (func $f2v (param f32)) (export "f2v" $f2v))').exports;
var i2vm = new Module(wasmTextToBinary('(module (import "a" "b" (param i32)))'));
var f2vm = new Module(wasmTextToBinary('(module (import "a" "b" (param f32)))'));
assertEq(new Instance(i2vm, {a:{b:e.i2v}}) instanceof Instance, true);
assertErrorMessage(() => new Instance(i2vm, {a:{b:e.f2v}}), TypeError, /imported function signature mismatch/);
assertErrorMessage(() => new Instance(f2vm, {a:{b:e.i2v}}), TypeError, /imported function signature mismatch/);
assertEq(new Instance(f2vm, {a:{b:e.f2v}}) instanceof Instance, true);

// Import order:

var arr = [];
var importObj = {
    get foo() { arr.push("foo") },
    get baz() { arr.push("bad") },
};
assertErrorMessage(() => new Instance(m1, importObj), TypeError, /import object field is not an Object/);
assertEq(arr.join(), "foo");

var arr = [];
var importObj = {
    get foo() {
        arr.push("foo");
        return { get bar() { arr.push("bar"); return null } }
    },
    get baz() { arr.push("bad") },
};
assertErrorMessage(() => new Instance(m1, importObj), TypeError, /import object field is not a Function/);
assertEq(arr.join(), "foo,bar");

var arr = [];
var importObj = {
    get foo() {
        arr.push("foo");
        return { get bar() { arr.push("bar"); return () => arr.push("bad") } }
    },
    get baz() {
        arr.push("baz");
        return { get quux() { arr.push("quux"); return () => arr.push("bad") } }
    }
};
assertEq(new Instance(m1, importObj) instanceof Instance, true);
assertEq(arr.join(), "foo,bar,baz,quux");

var arr = [];
var importObj = {
    get foo() {
        arr.push("foo");
        return { get bar() { arr.push("bar"); return new WebAssembly.Memory({initial:1, maximum:1}) } }
    },
    get baz() {
        arr.push("baz");
        return { get quux() { arr.push("quux"); return () => arr.push("bad") } }
    }
};
assertEq(new Instance(m3, importObj) instanceof Instance, true);
assertEq(arr.join(), "foo,bar,baz,quux");
arr = [];
assertEq(new Instance(m4, importObj) instanceof Instance, true);
assertEq(arr.join(), "baz,quux,foo,bar");

// Export key order:

var code = wasmTextToBinary('(module)');
var e = new Instance(new Module(code)).exports;
assertEq(Object.keys(e).length, 0);

var code = wasmTextToBinary('(module (func) (export "foo" 0))');
var e = new Instance(new Module(code)).exports;
assertEq(Object.keys(e).join(), "foo");
assertEq(e.foo(), undefined);

var code = wasmTextToBinary('(module (func) (export "foo" 0) (export "bar" 0))');
var e = new Instance(new Module(code)).exports;
assertEq(Object.keys(e).join(), "foo,bar");
assertEq(e.foo(), undefined);
assertEq(e.bar(), undefined);
assertEq(e.foo, e.bar);

var code = wasmTextToBinary('(module (memory 1 1) (export "memory" memory))');
var e = new Instance(new Module(code)).exports;
assertEq(Object.keys(e).join(), "memory");

var code = wasmTextToBinary('(module (memory 1 1) (export "foo" memory) (export "bar" memory))');
var e = new Instance(new Module(code)).exports;
assertEq(Object.keys(e).join(), "foo,bar");
assertEq(e.foo, e.bar);
assertEq(e.foo instanceof Memory, true);
assertEq(e.foo.buffer.byteLength, 64*1024);

var code = wasmTextToBinary('(module (memory 1 1) (func) (export "foo" 0) (export "bar" memory))');
var e = new Instance(new Module(code)).exports;
assertEq(Object.keys(e).join(), "foo,bar");
assertEq(e.foo(), undefined);
assertEq(e.bar instanceof Memory, true);
assertEq(e.bar instanceof Memory, true);
assertEq(e.bar.buffer.byteLength, 64*1024);

var code = wasmTextToBinary('(module (memory 1 1) (func) (export "bar" memory) (export "foo" 0))');
var e = new Instance(new Module(code)).exports;
assertEq(Object.keys(e).join(), "bar,foo");
assertEq(e.foo(), undefined);
assertEq(e.bar.buffer.byteLength, 64*1024);

var code = wasmTextToBinary('(module (memory 1 1) (export "" memory))');
var e = new Instance(new Module(code)).exports;
assertEq(Object.keys(e).length, 1);
assertEq(String(Object.keys(e)), "");
assertEq(e[""] instanceof Memory, true);

var code = wasmTextToBinary('(module (table) (export "tbl" table))');
var e = new Instance(new Module(code)).exports;
assertEq(Object.keys(e).join(), "tbl");
assertEq(e.tbl instanceof Table, true);
assertEq(e.tbl.length, 0);

var code = wasmTextToBinary('(module (table (resizable 2)) (export "t1" table) (export "t2" table))');
var e = new Instance(new Module(code)).exports;
assertEq(Object.keys(e).join(), "t1,t2");
assertEq(e.t1 instanceof Table, true);
assertEq(e.t2 instanceof Table, true);
assertEq(e.t1, e.t2);
assertEq(e.t1.length, 2);

var code = wasmTextToBinary('(module (table (resizable 2)) (memory 1 1) (func) (export "t" table) (export "m" memory) (export "f" 0))');
var e = new Instance(new Module(code)).exports;
assertEq(Object.keys(e).join(), "t,m,f");
assertEq(e.f(), undefined);
assertEq(e.t instanceof Table, true);
assertEq(e.m instanceof Memory, true);
assertEq(e.t.length, 2);

var code = wasmTextToBinary('(module (table (resizable 1)) (memory 1 1) (func) (export "m" memory) (export "f" 0) (export "t" table))');
var e = new Instance(new Module(code)).exports;
assertEq(Object.keys(e).join(), "m,f,t");
assertEq(e.f(), undefined);
assertEq(e.t instanceof Table, true);
assertEq(e.m instanceof Memory, true);
+assertEq(e.t.length, 1);

var code = wasmTextToBinary('(module (table) (export "" table))');
var e = new Instance(new Module(code)).exports;
assertEq(Object.keys(e).length, 1);
assertEq(String(Object.keys(e)), "");
assertEq(e[""] instanceof Table, true);
+assertEq(e[""].length, 0);

// Table export function identity

var code = wasmTextToBinary(`(module
    (func $f (result i32) (i32.const 1))
    (func $g (result i32) (i32.const 2))
    (func $h (result i32) (i32.const 3))
    (table (resizable 4))
    (elem (i32.const 0) $f)
    (elem (i32.const 2) $g)
    (export "f1" $f)
    (export "tbl1" table)
    (export "f2" $f)
    (export "tbl2" table)
    (export "f3" $h)
)`);
var e = new Instance(new Module(code)).exports;
assertEq(String(Object.keys(e)), "f1,tbl1,f2,tbl2,f3");
assertEq(e.f1, e.f2);
assertEq(e.f1(), 1);
assertEq(e.f3(), 3);
assertEq(e.tbl1, e.tbl2);
assertEq(e.tbl1.get(0), e.f1);
assertEq(e.tbl1.get(0), e.tbl1.get(0));
assertEq(e.tbl1.get(0)(), 1);
assertEq(e.tbl1.get(1), null);
assertEq(e.tbl1.get(2), e.tbl1.get(2));
assertEq(e.tbl1.get(2)(), 2);
assertEq(e.tbl1.get(3), null);
assertErrorMessage(() => e.tbl1.get(4), RangeError, /bad Table get index/);
assertEq(e.tbl1.get(1), null);
e.tbl1.set(1, e.f3);
assertEq(e.tbl1.get(1), e.f3);
e.tbl1.set(1, null);
assertEq(e.tbl1.get(1), null);
e.tbl1.set(3, e.f1);
assertEq(e.tbl1.get(0), e.tbl1.get(3));

// Re-exports and Identity:

var code = wasmTextToBinary('(module (import "a" "b" (memory 1 1)) (export "foo" memory) (export "bar" memory))');
var mem = new Memory({initial:1, maximum:1});
var e = new Instance(new Module(code), {a:{b:mem}}).exports;
assertEq(mem, e.foo);
assertEq(mem, e.bar);

var code = wasmTextToBinary('(module (import "a" "b" (table 1 1)) (export "foo" table) (export "bar" table))');
var tbl = new Table({initial:1, maximum:1, element:"anyfunc"});
var e = new Instance(new Module(code), {a:{b:tbl}}).exports;
assertEq(tbl, e.foo);
assertEq(tbl, e.bar);

var code = wasmTextToBinary('(module (import "a" "b" (table 2 2)) (func $foo) (elem (i32.const 0) $foo) (export "foo" $foo))');
var tbl = new Table({initial:2, maximum:2, element:"anyfunc"});
var e1 = new Instance(new Module(code), {a:{b:tbl}}).exports;
assertEq(e1.foo, tbl.get(0));
tbl.set(1, e1.foo);
assertEq(e1.foo, tbl.get(1));
var e2 = new Instance(new Module(code), {a:{b:tbl}}).exports;
assertEq(e2.foo, tbl.get(0));
assertEq(e1.foo, tbl.get(1));
assertEq(tbl.get(0) === e1.foo, false);
assertEq(e1.foo === e2.foo, false);

var code = wasmTextToBinary('(module (table (resizable 2 2)) (import $foo "a" "b" (result i32)) (func $bar (result i32) (i32.const 13)) (elem (i32.const 0) $foo $bar) (export "foo" $foo) (export "bar" $bar) (export "tbl" table))');
var foo = new Instance(new Module(wasmTextToBinary('(module (func (result i32) (i32.const 42)) (export "foo" 0))'))).exports.foo;
var e1 = new Instance(new Module(code), {a:{b:foo}}).exports;
assertEq(foo, e1.foo);
assertEq(foo, e1.tbl.get(0));
assertEq(e1.bar, e1.tbl.get(1));
assertEq(e1.tbl.get(0)(), 42);
assertEq(e1.tbl.get(1)(), 13);
var e2 = new Instance(new Module(code), {a:{b:foo}}).exports;
assertEq(e1.foo, e2.foo);
assertEq(e1.bar === e2.bar, false);
assertEq(e1.tbl === e2.tbl, false);
assertEq(e1.tbl.get(0), e2.tbl.get(0));
assertEq(e1.tbl.get(1) === e2.tbl.get(1), false);

// i64 is fully allowed for imported wasm functions

var code1 = wasmTextToBinary('(module (func $exp (param i64) (result i64) (i64.add (get_local 0) (i64.const 10))) (export "exp" $exp))');
var e1 = new Instance(new Module(code1)).exports;
var code2 = wasmTextToBinary('(module (import $i "a" "b" (param i64) (result i64)) (func $f (result i32) (i32.wrap/i64 (call_import $i (i64.const 42)))) (export "f" $f))');
var e2 = new Instance(new Module(code2), {a:{b:e1.exp}}).exports;
assertEq(e2.f(), 52);

// Non-existent export errors

wasmFailValidateText('(module (export "a" 0))', /exported function index out of bounds/);
wasmFailValidateText('(module (export "a" global 0))', /exported global index out of bounds/);
wasmFailValidateText('(module (export "a" memory))', /exported memory index out of bounds/);
wasmFailValidateText('(module (export "a" table))', /exported table index out of bounds/);

// Default memory/table rules

wasmFailValidateText('(module (import "a" "b" (memory 1 1)) (memory 1 1))', /already have default memory/);
wasmFailValidateText('(module (import "a" "b" (memory 1 1)) (import "x" "y" (memory 2 2)))', /already have default memory/);
wasmFailValidateText('(module (import "a" "b" (table 1 1)) (table 1 1))', /already have default table/);
wasmFailValidateText('(module (import "a" "b" (table 1 1)) (import "x" "y" (table 2 2)))', /already have default table/);

// Data segments on imports

var m = new Module(wasmTextToBinary(`
    (module
        (import "a" "b" (memory 1 1))
        (data 0 "\\0a\\0b")
        (data 100 "\\0c\\0d")
        (func $get (param $p i32) (result i32)
            (i32.load8_u (get_local $p)))
        (export "get" $get))
`));
var mem = new Memory({initial:1, maximum:1});
var {get} = new Instance(m, {a:{b:mem}}).exports;
assertEq(get(0), 0xa);
assertEq(get(1), 0xb);
assertEq(get(2), 0x0);
assertEq(get(100), 0xc);
assertEq(get(101), 0xd);
assertEq(get(102), 0x0);
var i8 = new Uint8Array(mem.buffer);
assertEq(i8[0], 0xa);
assertEq(i8[1], 0xb);
assertEq(i8[2], 0x0);
assertEq(i8[100], 0xc);
assertEq(i8[101], 0xd);
assertEq(i8[102], 0x0);

// Data segments with imported offsets

var m = new Module(wasmTextToBinary(`
    (module
        (import "glob" "a" (global i32 immutable))
        (memory 1)
        (data (get_global 0) "\\0a\\0b"))
`));
assertEq(new Instance(m, {glob:{a:0}}) instanceof Instance, true);
assertEq(new Instance(m, {glob:{a:(64*1024 - 2)}}) instanceof Instance, true);
assertErrorMessage(() => new Instance(m, {glob:{a:(64*1024 - 1)}}), RangeError, /data segment does not fit/);
assertErrorMessage(() => new Instance(m, {glob:{a:64*1024}}), RangeError, /data segment does not fit/);

// Errors during segment initialization do not have observable effects
// and are checked against the actual memory/table length, not the declared
// initial length.

var m = new Module(wasmTextToBinary(`
    (module
        (import "a" "mem" (memory 1))
        (import "a" "tbl" (table 1))
        (import $memOff "a" "memOff" (global i32 immutable))
        (import $tblOff "a" "tblOff" (global i32 immutable))
        (func $f)
        (func $g)
        (data (i32.const 0) "\\01")
        (elem (i32.const 0) $f)
        (data (get_global $memOff) "\\02")
        (elem (get_global $tblOff) $g)
        (export "f" $f)
        (export "g" $g))
`));

var npages = 2;
var mem = new Memory({initial:npages});
var mem8 = new Uint8Array(mem.buffer);
var tbl = new Table({initial:2, element:"anyfunc"});

assertErrorMessage(() => new Instance(m, {a:{mem, tbl, memOff:1, tblOff:2}}), RangeError, /elem segment does not fit/);
assertEq(mem8[0], 0);
assertEq(mem8[1], 0);
assertEq(tbl.get(0), null);

assertErrorMessage(() => new Instance(m, {a:{mem, tbl, memOff:npages*64*1024, tblOff:1}}), RangeError, /data segment does not fit/);
assertEq(mem8[0], 0);
assertEq(tbl.get(0), null);
assertEq(tbl.get(1), null);

var i = new Instance(m, {a:{mem, tbl, memOff:npages*64*1024-1, tblOff:1}});
assertEq(mem8[0], 1);
assertEq(mem8[npages*64*1024-1], 2);
assertEq(tbl.get(0), i.exports.f);
assertEq(tbl.get(1), i.exports.g);

// Elem segments on imports

var m = new Module(wasmTextToBinary(`
    (module
        (import "a" "b" (table 10))
        (elem (i32.const 0) $one $two)
        (elem (i32.const 3) $three $four)
        (func $one (result i32) (i32.const 1))
        (func $two (result i32) (i32.const 2))
        (func $three (result i32) (i32.const 3))
        (func $four (result i32) (i32.const 4)))
`));
var tbl = new Table({initial:10, element:"anyfunc"});
new Instance(m, {a:{b:tbl}});
assertEq(tbl.get(0)(), 1);
assertEq(tbl.get(1)(), 2);
assertEq(tbl.get(2), null);
assertEq(tbl.get(3)(), 3);
assertEq(tbl.get(4)(), 4);
for (var i = 5; i < 10; i++)
    assertEq(tbl.get(i), null);

// Cross-instance calls

var i1 = new Instance(new Module(wasmTextToBinary(`(module (func) (func (param i32) (result i32) (i32.add (get_local 0) (i32.const 1))) (func) (export "f" 1))`)));
var i2 = new Instance(new Module(wasmTextToBinary(`(module (import $imp "a" "b" (param i32) (result i32)) (func $g (result i32) (call $imp (i32.const 13))) (export "g" $g))`)), {a:{b:i1.exports.f}});
assertEq(i2.exports.g(), 14);

var i1 = new Instance(new Module(wasmTextToBinary(`(module
    (memory 1 1)
    (data (i32.const 0) "\\42")
    (func $f (result i32) (i32.load (i32.const 0)))
    (export "f" $f)
)`)));
var i2 = new Instance(new Module(wasmTextToBinary(`(module
    (import $imp "a" "b" (result i32))
    (memory 1 1)
    (data (i32.const 0) "\\13")
    (table (resizable 2 2))
    (elem (i32.const 0) $imp $def)
    (func $def (result i32) (i32.load (i32.const 0)))
    (type $v2i (func (result i32)))
    (func $call (param i32) (result i32) (call_indirect $v2i (get_local 0)))
    (export "call" $call)
)`)), {a:{b:i1.exports.f}});
assertEq(i2.exports.call(0), 0x42);
assertEq(i2.exports.call(1), 0x13);

var m = new Module(wasmTextToBinary(`(module
    (import $val "a" "val" (global i32 immutable))
    (import $next "a" "next" (result i32))
    (memory 1)
    (func $start (i32.store (i32.const 0) (get_global $val)))
    (start $start)
    (func $call (result i32)
        (i32.add
            (get_global $val)
            (i32.add
                (i32.load (i32.const 0))
                (call $next))))
    (export "call" $call)
)`));
var e = {call:() => 1000};
for (var i = 0; i < 10; i++)
    e = new Instance(m, {a:{val:i, next:e.call}}).exports;
assertEq(e.call(), 1090);
