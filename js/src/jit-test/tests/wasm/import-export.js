const { Module, Instance, Memory, Table, LinkError, RuntimeError } = WebAssembly;

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

function assertSegmentFitError(f) {
    assertErrorMessage(f, RuntimeError, /out of bounds/);
}

const m1 = new Module(wasmTextToBinary('(module (import "foo" "bar" (func)) (import "baz" "quux" (func)))'));
assertErrorMessage(() => new Instance(m1), TypeError, /second argument must be an object/);
assertErrorMessage(() => new Instance(m1, {foo:null}), TypeError, /import object field 'foo' is not an Object/);
assertErrorMessage(() => new Instance(m1, {foo:{bar:{}}}), LinkError, /import object field 'bar' is not a Function/);
assertErrorMessage(() => new Instance(m1, {foo:{bar:()=>{}}, baz:null}), TypeError, /import object field 'baz' is not an Object/);
assertErrorMessage(() => new Instance(m1, {foo:{bar:()=>{}}, baz:{}}), LinkError, /import object field 'quux' is not a Function/);
assertEq(new Instance(m1, {foo:{bar:()=>{}}, baz:{quux:()=>{}}}) instanceof Instance, true);

const m2 = new Module(wasmTextToBinary('(module (import "x" "y" (memory 2 3)))'));
assertErrorMessage(() => new Instance(m2), TypeError, /second argument must be an object/);
assertErrorMessage(() => new Instance(m2, {x:null}), TypeError, /import object field 'x' is not an Object/);
assertErrorMessage(() => new Instance(m2, {x:{y:{}}}), LinkError, /import object field 'y' is not a Memory/);
assertErrorMessage(() => new Instance(m2, {x:{y:mem1Page}}), LinkError, /imported Memory with incompatible size/);
assertErrorMessage(() => new Instance(m2, {x:{y:mem1PageMax1}}), LinkError, /imported Memory with incompatible size/);
assertErrorMessage(() => new Instance(m2, {x:{y:mem4Page}}), LinkError, /imported Memory with incompatible size/);
assertErrorMessage(() => new Instance(m2, {x:{y:mem4PageMax4}}), LinkError, /imported Memory with incompatible size/);
assertErrorMessage(() => new Instance(m2, {x:{y:mem2Page}}), LinkError, /imported Memory with incompatible maximum size/);
assertEq(new Instance(m2, {x:{y:mem2PageMax2}}) instanceof Instance, true);
assertErrorMessage(() => new Instance(m2, {x:{y:mem3Page}}), LinkError, /imported Memory with incompatible maximum size/);
assertEq(new Instance(m2, {x:{y:mem3PageMax3}}) instanceof Instance, true);
assertEq(new Instance(m2, {x:{y:mem2PageMax3}}) instanceof Instance, true);
assertErrorMessage(() => new Instance(m2, {x:{y:mem2PageMax4}}), LinkError, /imported Memory with incompatible maximum size/);

const m3 = new Module(wasmTextToBinary('(module (import "foo" "bar" (memory 1 1)) (import "baz" "quux" (func)))'));
assertErrorMessage(() => new Instance(m3), TypeError, /second argument must be an object/);
assertErrorMessage(() => new Instance(m3, {foo:null}), TypeError, /import object field 'foo' is not an Object/);
assertErrorMessage(() => new Instance(m3, {foo:{bar:{}}}), LinkError, /import object field 'bar' is not a Memory/);
assertErrorMessage(() => new Instance(m3, {foo:{bar:mem1Page}, baz:null}), TypeError, /import object field 'baz' is not an Object/);
assertErrorMessage(() => new Instance(m3, {foo:{bar:mem1Page}, baz:{quux:mem1Page}}), LinkError, /import object field 'quux' is not a Function/);
assertErrorMessage(() => new Instance(m3, {foo:{bar:mem1Page}, baz:{quux:()=>{}}}), LinkError, /imported Memory with incompatible maximum size/);
assertEq(new Instance(m3, {foo:{bar:mem1PageMax1}, baz:{quux:()=>{}}}) instanceof Instance, true);

const m4 = new Module(wasmTextToBinary('(module (import "baz" "quux" (func)) (import "foo" "bar" (memory 1 1)))'));
assertErrorMessage(() => new Instance(m4), TypeError, /second argument must be an object/);
assertErrorMessage(() => new Instance(m4, {baz:null}), TypeError, /import object field 'baz' is not an Object/);
assertErrorMessage(() => new Instance(m4, {baz:{quux:{}}}), LinkError, /import object field 'quux' is not a Function/);
assertErrorMessage(() => new Instance(m4, {baz:{quux:()=>{}}, foo:null}), TypeError, /import object field 'foo' is not an Object/);
assertErrorMessage(() => new Instance(m4, {baz:{quux:()=>{}}, foo:{bar:()=>{}}}), LinkError, /import object field 'bar' is not a Memory/);
assertErrorMessage(() => new Instance(m4, {baz:{quux:()=>{}}, foo:{bar:mem1Page}}), LinkError, /imported Memory with incompatible maximum size/);
assertEq(new Instance(m3, {baz:{quux:()=>{}}, foo:{bar:mem1PageMax1}}) instanceof Instance, true);

const m5 = new Module(wasmTextToBinary('(module (import "a" "b" (memory 2)))'));
assertErrorMessage(() => new Instance(m5, {a:{b:mem1Page}}), LinkError, /imported Memory with incompatible size/);
assertEq(new Instance(m5, {a:{b:mem2Page}}) instanceof Instance, true);
assertEq(new Instance(m5, {a:{b:mem3Page}}) instanceof Instance, true);
assertEq(new Instance(m5, {a:{b:mem4Page}}) instanceof Instance, true);

const m6 = new Module(wasmTextToBinary('(module (import "a" "b" (table 2 funcref)))'));
assertErrorMessage(() => new Instance(m6, {a:{b:tab1Elem}}), LinkError, /imported Table with incompatible size/);
assertEq(new Instance(m6, {a:{b:tab2Elem}}) instanceof Instance, true);
assertEq(new Instance(m6, {a:{b:tab3Elem}}) instanceof Instance, true);
assertEq(new Instance(m6, {a:{b:tab4Elem}}) instanceof Instance, true);

const m7 = new Module(wasmTextToBinary('(module (import "a" "b" (table 2 3 funcref)))'));
assertErrorMessage(() => new Instance(m7, {a:{b:tab1Elem}}), LinkError, /imported Table with incompatible size/);
assertErrorMessage(() => new Instance(m7, {a:{b:tab2Elem}}), LinkError, /imported Table with incompatible maximum size/);
assertErrorMessage(() => new Instance(m7, {a:{b:tab3Elem}}), LinkError, /imported Table with incompatible maximum size/);
assertErrorMessage(() => new Instance(m7, {a:{b:tab4Elem}}), LinkError, /imported Table with incompatible size/);

wasmFailValidateText('(module (memory 2 1))', /maximum length 1 is less than initial length 2/);
wasmFailValidateText('(module (import "a" "b" (memory 2 1)))', /maximum length 1 is less than initial length 2/);
wasmFailValidateText('(module (table 2 1 funcref))', /maximum length 1 is less than initial length 2/);
wasmFailValidateText('(module (import "a" "b" (table 2 1 funcref)))', /maximum length 1 is less than initial length 2/);

// Import wasm-wasm type mismatch

var e = wasmEvalText('(module (func $i2v (param i32)) (export "i2v" (func $i2v)) (func $f2v (param f32)) (export "f2v" (func $f2v)))').exports;
var i2vm = new Module(wasmTextToBinary('(module (import "a" "b" (func (param i32))))'));
var f2vm = new Module(wasmTextToBinary('(module (import "a" "b" (func (param f32))))'));
assertEq(new Instance(i2vm, {a:{b:e.i2v}}) instanceof Instance, true);
assertErrorMessage(() => new Instance(i2vm, {a:{b:e.f2v}}), LinkError, /imported function 'a.b' signature mismatch/);
assertErrorMessage(() => new Instance(f2vm, {a:{b:e.i2v}}), LinkError, /imported function 'a.b' signature mismatch/);
assertEq(new Instance(f2vm, {a:{b:e.f2v}}) instanceof Instance, true);
var l2vm = new Module(wasmTextToBinary('(module (import "x" "y" (memory 1)) (import "c" "d" (func (param i64))))'));
assertErrorMessage(() => new Instance(l2vm, {x:{y:mem1Page}, c:{d:e.i2v}}), LinkError, /imported function 'c.d' signature mismatch/);

// Import order:

var arr = [];
var importObj = {
    get foo() { arr.push("foo") },
    get baz() { arr.push("bad") },
};
assertErrorMessage(() => new Instance(m1, importObj), TypeError, /import object field 'foo' is not an Object/);
assertEq(arr.join(), "foo");

var arr = [];
var importObj = {
    get foo() {
        arr.push("foo");
        return { get bar() { arr.push("bar"); return null } }
    },
    get baz() { arr.push("bad") },
};
assertErrorMessage(() => new Instance(m1, importObj), LinkError, /import object field 'bar' is not a Function/);
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

var code = wasmTextToBinary('(module (func) (export "foo" (func 0)))');
var e = new Instance(new Module(code)).exports;
assertEq(Object.keys(e).join(), "foo");
assertEq(e.foo(), undefined);

var code = wasmTextToBinary('(module (func) (export "foo" (func 0)) (export "bar" (func 0)))');
var e = new Instance(new Module(code)).exports;
assertEq(Object.keys(e).join(), "foo,bar");
assertEq(e.foo(), undefined);
assertEq(e.bar(), undefined);
assertEq(e.foo, e.bar);

var code = wasmTextToBinary('(module (memory 1 1) (export "memory" (memory 0)))');
var e = new Instance(new Module(code)).exports;
assertEq(Object.keys(e).join(), "memory");

var code = wasmTextToBinary('(module (memory 1 1) (export "foo" (memory 0)) (export "bar" (memory 0)))');
var e = new Instance(new Module(code)).exports;
assertEq(Object.keys(e).join(), "foo,bar");
assertEq(e.foo, e.bar);
assertEq(e.foo instanceof Memory, true);
assertEq(e.foo.buffer.byteLength, 64*1024);

var code = wasmTextToBinary('(module (memory 1 1) (func) (export "foo" (func 0)) (export "bar" (memory 0)))');
var e = new Instance(new Module(code)).exports;
assertEq(Object.keys(e).join(), "foo,bar");
assertEq(e.foo(), undefined);
assertEq(e.bar instanceof Memory, true);
assertEq(e.bar instanceof Memory, true);
assertEq(e.bar.buffer.byteLength, 64*1024);

var code = wasmTextToBinary('(module (memory 1 1) (func) (export "bar" (memory 0)) (export "foo" (func 0)))');
var e = new Instance(new Module(code)).exports;
assertEq(Object.keys(e).join(), "bar,foo");
assertEq(e.foo(), undefined);
assertEq(e.bar.buffer.byteLength, 64*1024);

var code = wasmTextToBinary('(module (memory 1 1) (export "" (memory 0)))');
var e = new Instance(new Module(code)).exports;
assertEq(Object.keys(e).length, 1);
assertEq(String(Object.keys(e)), "");
assertEq(e[""] instanceof Memory, true);

var code = wasmTextToBinary('(module (table 0 funcref) (export "tbl" (table 0)))');
var e = new Instance(new Module(code)).exports;
assertEq(Object.keys(e).join(), "tbl");
assertEq(e.tbl instanceof Table, true);
assertEq(e.tbl.length, 0);

var code = wasmTextToBinary('(module (table 2 funcref) (export "t1" (table 0)) (export "t2" (table 0)))');
var e = new Instance(new Module(code)).exports;
assertEq(Object.keys(e).join(), "t1,t2");
assertEq(e.t1 instanceof Table, true);
assertEq(e.t2 instanceof Table, true);
assertEq(e.t1, e.t2);
assertEq(e.t1.length, 2);

var code = wasmTextToBinary('(module (table 2 funcref) (memory 1 1) (func) (export "t" (table 0)) (export "m" (memory 0)) (export "f" (func 0)))');
var e = new Instance(new Module(code)).exports;
assertEq(Object.keys(e).join(), "t,m,f");
assertEq(e.f(), undefined);
assertEq(e.t instanceof Table, true);
assertEq(e.m instanceof Memory, true);
assertEq(e.t.length, 2);

var code = wasmTextToBinary('(module (table 1 funcref) (memory 1 1) (func) (export "m" (memory 0)) (export "f" (func 0)) (export "t" (table 0)))');
var e = new Instance(new Module(code)).exports;
assertEq(Object.keys(e).join(), "m,f,t");
assertEq(e.f(), undefined);
assertEq(e.t instanceof Table, true);
assertEq(e.m instanceof Memory, true);
+assertEq(e.t.length, 1);

var code = wasmTextToBinary('(module (table 0 funcref) (export "" (table 0)))');
var e = new Instance(new Module(code)).exports;
assertEq(Object.keys(e).length, 1);
assertEq(String(Object.keys(e)), "");
assertEq(e[""] instanceof Table, true);
+assertEq(e[""].length, 0);

// Table export function identity

var text = `(module
    (func $f (result i32) (i32.const 1))
    (func $g (result i32) (i32.const 2))
    (func $h (result i32) (i32.const 3))
    (table 4 funcref)
    (elem (i32.const 0) $f)
    (elem (i32.const 2) $g)
    (export "f1" (func $f))
    (export "tbl1" (table 0))
    (export "f2" (func $f))
    (export "tbl2" (table 0))
    (export "f3" (func $h))
    (func (export "run") (result i32) (call_indirect (type 0) (i32.const 2)))
)`;
wasmFullPass(text, 2);
var e = new Instance(new Module(wasmTextToBinary(text))).exports;
assertEq(String(Object.keys(e)), "f1,tbl1,f2,tbl2,f3,run");
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

// JS re-exports

var args;
var m = new Module(wasmTextToBinary(`(module
    (export "a" (func $a)) (import "" "a" (func $a (param f32)))
    (export "b" (func $b)) (import "" "b" (func $b (param i32) (result i32)))
    (export "c" (func $c)) (import "" "c" (func $c (result i32)))
    (export "d" (func $d)) (import "" "d" (func $d))
)`));
var js = function() { args = arguments; return 42 }
var e = new Instance(m, {"":{a:js, b:js, c:js, d:js}}).exports;
assertEq(e.a.length, 1);
assertEq(e.a(), undefined);
assertEq(args.length, 1);
assertEq(args[0], NaN);
assertEq(e.a(99.5), undefined);
assertEq(args.length, 1);
assertEq(args[0], 99.5);
assertEq(e.b.length, 1);
assertEq(e.b(), 42);
assertEq(args.length, 1);
assertEq(args[0], 0);
assertEq(e.b(99.5), 42);
assertEq(args.length, 1);
assertEq(args[0], 99);
assertEq(e.c.length, 0);
assertEq(e.c(), 42);
assertEq(args.length, 0);
assertEq(e.c(99), 42);
assertEq(args.length, 0);
assertEq(e.d.length, 0);
assertEq(e.d(), undefined);
assertEq(args.length, 0);
assertEq(e.d(99), undefined);
assertEq(args.length, 0);

// Re-exports and Identity:

var code = wasmTextToBinary('(module (import "a" "b" (memory 1 1)) (export "foo" (memory 0)) (export "bar" (memory 0)))');
var mem = new Memory({initial:1, maximum:1});
var e = new Instance(new Module(code), {a:{b:mem}}).exports;
assertEq(mem, e.foo);
assertEq(mem, e.bar);

var code = wasmTextToBinary('(module (import "a" "b" (table 1 1 funcref)) (export "foo" (table 0)) (export "bar" (table 0)))');
var tbl = new Table({initial:1, maximum:1, element:"anyfunc"});
var e = new Instance(new Module(code), {a:{b:tbl}}).exports;
assertEq(tbl, e.foo);
assertEq(tbl, e.bar);

var code = wasmTextToBinary('(module (import "a" "b" (table 2 2 funcref)) (func $foo) (elem (i32.const 0) $foo) (export "foo" (func $foo)))');
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

var m = new Module(wasmTextToBinary(`(module
    (import "" "foo" (func $foo (result i32)))
    (import "" "bar" (func $bar (result i32)))
    (table 3 funcref)
    (func $baz (result i32) (i32.const 13))
    (elem (i32.const 0) $foo $bar $baz)
    (export "foo" (func $foo))
    (export "bar" (func $bar))
    (export "baz" (func $baz))
    (export "tbl" (table 0))
)`));
var jsFun = () => 83;
var wasmFun = new Instance(new Module(wasmTextToBinary('(module (func (result i32) (i32.const 42)) (export "foo" (func 0)))'))).exports.foo;
var e1 = new Instance(m, {"":{foo:jsFun, bar:wasmFun}}).exports;
assertEq(jsFun === e1.foo, false);
assertEq(wasmFun, e1.bar);
assertEq(e1.foo, e1.tbl.get(0));
assertEq(e1.bar, e1.tbl.get(1));
assertEq(e1.baz, e1.tbl.get(2));
assertEq(e1.tbl.get(0)(), 83);
assertEq(e1.tbl.get(1)(), 42);
assertEq(e1.tbl.get(2)(), 13);
var e2 = new Instance(m, {"":{foo:jsFun, bar:jsFun}}).exports;
assertEq(jsFun === e2.foo, false);
assertEq(jsFun === e2.bar, false);
assertEq(e2.foo === e1.foo, false);
assertEq(e2.bar === e1.bar, false);
assertEq(e2.baz === e1.baz, false);
assertEq(e2.tbl === e1.tbl, false);
assertEq(e2.foo, e2.tbl.get(0));
assertEq(e2.bar, e2.tbl.get(1));
assertEq(e2.baz, e2.tbl.get(2));
var e3 = new Instance(m, {"":{foo:wasmFun, bar:wasmFun}}).exports;
assertEq(wasmFun, e3.foo);
assertEq(wasmFun, e3.bar);
assertEq(e3.baz === e3.foo, false);
assertEq(e3.baz === e1.baz, false);
assertEq(e3.tbl === e1.tbl, false);
assertEq(e3.foo, e3.tbl.get(0));
assertEq(e3.bar, e3.tbl.get(1));
assertEq(e3.baz, e3.tbl.get(2));
var e4 = new Instance(m, {"":{foo:e1.foo, bar:e1.foo}}).exports;
assertEq(e4.foo, e1.foo);
assertEq(e4.bar, e1.foo);
assertEq(e4.baz === e4.foo, false);
assertEq(e4.baz === e1.baz, false);
assertEq(e4.tbl === e1.tbl, false);
assertEq(e4.foo, e4.tbl.get(0));
assertEq(e4.foo, e4.tbl.get(1));
assertEq(e4.baz, e4.tbl.get(2));

// i64 is fully allowed for imported wasm functions

var code1 = wasmTextToBinary('(module (func $exp (param i64) (result i64) (i64.add (local.get 0) (i64.const 10))) (export "exp" (func $exp)))');
var e1 = new Instance(new Module(code1)).exports;
var code2 = wasmTextToBinary('(module (import "a" "b" (func $i (param i64) (result i64))) (func $f (result i32) (i32.wrap_i64 (call $i (i64.const 42)))) (export "f" (func $f)))');
var e2 = new Instance(new Module(code2), {a:{b:e1.exp}}).exports;
assertEq(e2.f(), 52);

// Non-existent export errors

wasmFailValidateText('(module (export "a" (func 0)))', /exported function index out of bounds/);
wasmFailValidateText('(module (export "a" (global 0)))', /exported global index out of bounds/);
wasmFailValidateText('(module (export "a" (memory 0)))', /exported memory index out of bounds/);
wasmFailValidateText('(module (export "a" (table 0)))', /exported table index out of bounds/);

// Default memory/table rules

wasmFailValidateText('(module (import "a" "b" (memory 1 1)) (memory 1 1))', /already have default memory/);
wasmFailValidateText('(module (import "a" "b" (memory 1 1)) (import "x" "y" (memory 2 2)))', /already have default memory/);

// Data segments on imports

var m = new Module(wasmTextToBinary(`
    (module
        (import "a" "b" (memory 1 1))
        (data (i32.const 0) "\\0a\\0b")
        (data (i32.const 100) "\\0c\\0d")
        (func $get (param $p i32) (result i32)
            (i32.load8_u (local.get $p)))
        (export "get" (func $get)))
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
        (import "glob" "a" (global i32))
        (memory 1)
        (data (global.get 0) "\\0a\\0b"))
`));
assertEq(new Instance(m, {glob:{a:0}}) instanceof Instance, true);
assertEq(new Instance(m, {glob:{a:(64*1024 - 2)}}) instanceof Instance, true);
assertSegmentFitError(() => new Instance(m, {glob:{a:(64*1024 - 1)}}));
assertSegmentFitError(() => new Instance(m, {glob:{a:64*1024}}));

var m = new Module(wasmTextToBinary(`
    (module
        (memory 1)
        (data (i32.const 0x10001) "\\0a\\0b"))
`));
assertSegmentFitError(() => new Instance(m));

var m = new Module(wasmTextToBinary(`
    (module
        (memory 0)
        (data (i32.const 0x10001) ""))
`));
assertSegmentFitError(() => new Instance(m));

// Errors during segment initialization do not have observable effects
// and are checked against the actual memory/table length, not the declared
// initial length.

var m = new Module(wasmTextToBinary(`
    (module
        (import "a" "mem" (memory 1))
        (import "a" "tbl" (table 1 funcref))
        (import "a" "memOff" (global $memOff i32))
        (import "a" "tblOff" (global $tblOff i32))
        (func $f)
        (func $g)
        (data (i32.const 0) "\\01")
        (elem (i32.const 0) $f)
        (data (global.get $memOff) "\\02")
        (elem (global.get $tblOff) $g)
        (export "f" (func $f))
        (export "g" (func $g)))
`));

// Active segments are applied in order (this is observable if they overlap).
//
// Without bulk memory, all range checking for tables and memory happens before
// any writes happen, and any OOB will force no writing to happen at all.
//
// With bulk memory, active segments are applied first for tables and then for
// memories.  Bounds checking happens for each byte or table element written.
// The first OOB aborts the initialization process, leaving written data in
// place.  Notably, any OOB in table initialization will prevent any memory
// initialization from happening at all.

var npages = 2;
var mem = new Memory({initial:npages});
var mem8 = new Uint8Array(mem.buffer);
var tbl = new Table({initial:2, element:"anyfunc"});

assertSegmentFitError(() => new Instance(m, {a:{mem, tbl, memOff:1, tblOff:2}}));
// The first active element segment is applied, but the second active
// element segment is completely OOB.
assertEq(typeof tbl.get(0), "function");
assertEq(tbl.get(1), null);

assertEq(mem8[0], 0);
assertEq(mem8[1], 0);

tbl.set(0, null);
tbl.set(1, null);

assertSegmentFitError(() => new Instance(m, {a:{mem, tbl, memOff:npages*64*1024, tblOff:1}}));
// The first and second active element segments are applied fully.  The
// first active data segment applies, but the second one is completely OOB.
assertEq(typeof tbl.get(0), "function");
assertEq(typeof tbl.get(1), "function");
assertEq(mem8[0], 1);

tbl.set(0, null);
tbl.set(1, null);
mem8[0] = 0;

// Both element and data segments apply successfully without OOB

var i = new Instance(m, {a:{mem, tbl, memOff:npages*64*1024-1, tblOff:1}});
assertEq(mem8[0], 1);
assertEq(mem8[npages*64*1024-1], 2);
assertEq(tbl.get(0), i.exports.f);
assertEq(tbl.get(1), i.exports.g);

// Element segment doesn't apply and prevents subsequent elem segment and
// data segment from being applied.

var m = new Module(wasmTextToBinary(
    `(module
       (import "" "mem" (memory 1))
       (import "" "tbl" (table 3 funcref))
       (elem (i32.const 1) $f $g $h) ;; fails after $f and $g
       (elem (i32.const 0) $f)       ;; is not applied
       (data (i32.const 0) "\\01")   ;; is not applied
       (func $f)
       (func $g)
       (func $h))`));
var mem = new Memory({initial:1});
var tbl = new Table({initial:3, element:"anyfunc"});
assertSegmentFitError(() => new Instance(m, {"":{mem, tbl}}));
assertEq(tbl.get(0), null);
assertEq(tbl.get(1), null);
assertEq(tbl.get(2), null);
var v = new Uint8Array(mem.buffer);
assertEq(v[0], 0);

// Data segment doesn't apply and prevents subsequent data segment from
// being applied.

var m = new Module(wasmTextToBinary(
    `(module
       (import "" "mem" (memory 1))
       (data (i32.const 65534) "\\01\\02\\03") ;; fails after 1 and 2
       (data (i32.const 0) "\\04")             ;; is not applied
     )`));
var mem = new Memory({initial:1});
assertSegmentFitError(() => new Instance(m, {"":{mem}}));
var v = new Uint8Array(mem.buffer);
assertEq(v[65534], 0);
assertEq(v[65535], 0);
assertEq(v[0], 0);

// Elem segments on imported tables

var m = new Module(wasmTextToBinary(`
    (module
        (import "a" "b" (table 10 funcref))
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

var m = new Module(wasmTextToBinary(`
    (module
        (func $their1 (import "" "func") (result i32))
        (func $their2 (import "" "func"))
        (table (import "" "table") 4 funcref)
        (func $my (result i32) i32.const 13)
        (elem (i32.const 1) $my)
        (elem (i32.const 2) $their1)
        (elem (i32.const 3) $their2)
    )
`));
var tbl = new Table({initial:4, element:"anyfunc"});
var f = () => 42;
new Instance(m, { "": { table: tbl, func: f} });
assertEq(tbl.get(0), null);
assertEq(tbl.get(1)(), 13);
assertEq(tbl.get(2)(), 42);
assertEq(tbl.get(3)(), undefined);

// Cross-instance calls

var i1 = new Instance(new Module(wasmTextToBinary(`(module (func) (func (param i32) (result i32) (i32.add (local.get 0) (i32.const 1))) (func) (export "f" (func 1)))`)));
var i2 = new Instance(new Module(wasmTextToBinary(`(module (import "a" "b" (func $imp (param i32) (result i32))) (func $g (result i32) (call $imp (i32.const 13))) (export "g" (func $g)))`)), {a:{b:i1.exports.f}});
assertEq(i2.exports.g(), 14);

var i1 = new Instance(new Module(wasmTextToBinary(`(module
    (memory 1 1)
    (data (i32.const 0) "\\42")
    (func $f (result i32) (i32.load (i32.const 0)))
    (export "f" (func $f))
)`)));
var i2 = new Instance(new Module(wasmTextToBinary(`(module
    (import "a" "b" (func $imp (result i32)))
    (memory 1 1)
    (data (i32.const 0) "\\13")
    (table 2 2 funcref)
    (elem (i32.const 0) $imp $def)
    (func $def (result i32) (i32.load (i32.const 0)))
    (type $v2i (func (result i32)))
    (func $call (param i32) (result i32) (call_indirect (type $v2i) (local.get 0)))
    (export "call" (func $call))
)`)), {a:{b:i1.exports.f}});
assertEq(i2.exports.call(0), 0x42);
assertEq(i2.exports.call(1), 0x13);

var m = new Module(wasmTextToBinary(`(module
    (import "a" "val" (global $val i32))
    (import "a" "next" (func $next (result i32)))
    (memory 1)
    (func $start (i32.store (i32.const 0) (global.get $val)))
    (start $start)
    (func $call (result i32)
        (i32.add
            (global.get $val)
            (i32.add
                (i32.load (i32.const 0))
                (call $next))))
    (export "call" (func $call))
)`));
var e = {call:() => 1000};
for (var i = 0; i < 10; i++)
    e = new Instance(m, {a:{val:i, next:e.call}}).exports;
assertEq(e.call(), 1090);

(function testImportJitExit() {
    let options = getJitCompilerOptions();
    if (!options['baseline.enable'])
        return;

    let baselineTrigger = options['baseline.warmup.trigger'];

    let valueToConvert = 0;
    function ffi(n) { if (n == 1337) { return valueToConvert }; return 42; }

    function sum(a, b, c) {
        if (a === 1337)
            return valueToConvert;
        return (a|0) + (b|0) + (c|0) | 0;
    }

    // Baseline compile ffis.
    for (let i = baselineTrigger + 1; i --> 0;) {
        ffi(i);
        sum((i%2)?i:undefined,
            (i%3)?i:undefined,
            (i%4)?i:undefined);
    }

    let imports = {
        a: {
            ffi,
            sum
        }
    };

    i = wasmEvalText(`(module
        (import "a" "ffi" (func $ffi (param i32) (result i32)))

        (import "a" "sum" (func $missingOneArg (param i32) (param i32) (result i32)))
        (import "a" "sum" (func $missingTwoArgs (param i32) (result i32)))
        (import "a" "sum" (func $missingThreeArgs (result i32)))

        (func (export "foo") (param i32) (result i32)
         local.get 0
         call $ffi
        )

        (func (export "missThree") (result i32)
         call $missingThreeArgs
        )

        (func (export "missTwo") (param i32) (result i32)
         local.get 0
         call $missingTwoArgs
        )

        (func (export "missOne") (param i32) (param i32) (result i32)
         local.get 0
         local.get 1
         call $missingOneArg
        )
    )`, imports).exports;

    // Enable the jit exit for each JS callee.
    assertEq(i.foo(0), 42);

    assertEq(i.missThree(), 0);
    assertEq(i.missTwo(42), 42);
    assertEq(i.missOne(13, 37), 50);

    // Test the jit exit under normal conditions.
    assertEq(i.foo(0), 42);
    assertEq(i.foo(1337), 0);

    // Test the arguments rectifier.
    assertEq(i.missThree(), 0);
    assertEq(i.missTwo(-1), -1);
    assertEq(i.missOne(23, 10), 33);

    // Test OOL coercion.
    valueToConvert = 2**31;
    assertEq(i.foo(1337), -(2**31));

    // Test OOL error path.
    valueToConvert = { valueOf() { throw new Error('make ffi great again'); } }
    assertErrorMessage(() => i.foo(1337), Error, "make ffi great again");

    valueToConvert = { toString() { throw new Error('a FFI to believe in'); } }
    assertErrorMessage(() => i.foo(1337), Error, "a FFI to believe in");

    // Test the error path in the arguments rectifier.
    assertErrorMessage(() => i.missTwo(1337), Error, "a FFI to believe in");
})();

(function testCrossRealmImport() {
    var g = newGlobal({sameCompartmentAs: this});
    g.evaluate("function f1() { assertCorrectRealm(); return 123; }");
    g.mem = new Memory({initial:8});

    // The memory.size builtin asserts cx->realm matches instance->realm so
    // we call it here.
    var i1 = new Instance(new Module(wasmTextToBinary(`
        (module
            (import "a" "f1" (func $imp1 (result i32)))
            (import "a" "f2" (func $imp2 (result i32)))
            (import "a" "m" (memory 1))
            (func $test (result i32)
                (i32.add
                    (i32.add
                        (i32.add (memory.size) (call $imp1))
                        (memory.size))
                    (call $imp2)))
            (export "impstub" (func $imp1))
            (export "test" (func $test)))
    `)), {a:{m:g.mem, f1:g.f1, f2:g.Math.abs}});

    for (var i = 0; i < 20; i++) {
        assertEq(i1.exports.impstub(), 123);
        assertEq(i1.exports.test(), 139);
    }

    // Inter-module/inter-realm wasm => wasm calls.
    var src = `
        (module
            (import "a" "othertest" (func $imp (result i32)))
            (import "a" "m" (memory 1))
            (func (result i32) (i32.add (call $imp) (memory.size)))
            (export "test" (func 1)))
    `;
    g.i1 = i1;
    g.evaluate("i2 = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`" + src + "`)), {a:{m:mem,othertest:i1.exports.test}})");
    for (var i = 0; i < 20; i++)
        assertEq(g.i2.exports.test(), 147);
})();

// The name presented in toString and as the fn.name property is the index of the
// function within the module.  See bug 1714505 for analysis.

var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module
  (func (export "myfunc") (result i32)
    (i32.const 1337))
  (func $hi (result i32)
    (i32.const 3))
  (func $abracadabra (export "bletch") (result i32)
    (i32.const -1)))`)))
assertEq(String(ins.exports.myfunc), "function 0() {\n    [native code]\n}")
assertEq(ins.exports.myfunc.name, "0");
assertEq(String(ins.exports.bletch), "function 2() {\n    [native code]\n}")
assertEq(ins.exports.bletch.name, "2")
