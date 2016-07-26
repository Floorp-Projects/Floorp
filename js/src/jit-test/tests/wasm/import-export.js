load(libdir + 'wasm.js');
load(libdir + 'asserts.js');

const Module = WebAssembly.Module;
const Instance = WebAssembly.Instance;
const Memory = WebAssembly.Memory;
const Table = WebAssembly.Table;

const mem1Page = new Memory({initial:1});
const mem2Page = new Memory({initial:2});
const mem3Page = new Memory({initial:3});
const mem4Page = new Memory({initial:4});
const tab1Elem = new Table({initial:1});
const tab2Elem = new Table({initial:2});
const tab3Elem = new Table({initial:3});
const tab4Elem = new Table({initial:4});

// Explicitly opt into the new binary format for imports and exports until it
// is used by default everywhere.
const textToBinary = str => wasmTextToBinary(str, 'new-format');

const m1 = new Module(textToBinary('(module (import "foo" "bar") (import "baz" "quux"))'));
assertErrorMessage(() => new Instance(m1), TypeError, /no import object given/);
assertErrorMessage(() => new Instance(m1, {foo:null}), TypeError, /import object field is not an Object/);
assertErrorMessage(() => new Instance(m1, {foo:{bar:{}}}), TypeError, /import object field is not a Function/);
assertErrorMessage(() => new Instance(m1, {foo:{bar:()=>{}}, baz:null}), TypeError, /import object field is not an Object/);
assertErrorMessage(() => new Instance(m1, {foo:{bar:()=>{}}, baz:{}}), TypeError, /import object field is not a Function/);
assertEq(new Instance(m1, {foo:{bar:()=>{}}, baz:{quux:()=>{}}}) instanceof Instance, true);

const m2 = new Module(textToBinary('(module (import "x" "y" (memory 2 3)))'));
assertErrorMessage(() => new Instance(m2), TypeError, /no import object given/);
assertErrorMessage(() => new Instance(m2, {x:null}), TypeError, /import object field is not an Object/);
assertErrorMessage(() => new Instance(m2, {x:{y:{}}}), TypeError, /import object field is not a Memory/);
assertErrorMessage(() => new Instance(m2, {x:{y:mem1Page}}), TypeError, /imported Memory with incompatible size/);
assertErrorMessage(() => new Instance(m2, {x:{y:mem4Page}}), TypeError, /imported Memory with incompatible size/);
assertEq(new Instance(m2, {x:{y:mem2Page}}) instanceof Instance, true);
assertEq(new Instance(m2, {x:{y:mem3Page}}) instanceof Instance, true);

const m3 = new Module(textToBinary('(module (import "foo" "bar" (memory 1 1)) (import "baz" "quux"))'));
assertErrorMessage(() => new Instance(m3), TypeError, /no import object given/);
assertErrorMessage(() => new Instance(m3, {foo:null}), TypeError, /import object field is not an Object/);
assertErrorMessage(() => new Instance(m3, {foo:{bar:{}}}), TypeError, /import object field is not a Memory/);
assertErrorMessage(() => new Instance(m3, {foo:{bar:mem1Page}, baz:null}), TypeError, /import object field is not an Object/);
assertErrorMessage(() => new Instance(m3, {foo:{bar:mem1Page}, baz:{quux:mem1Page}}), TypeError, /import object field is not a Function/);
assertEq(new Instance(m3, {foo:{bar:mem1Page}, baz:{quux:()=>{}}}) instanceof Instance, true);

const m4 = new Module(textToBinary('(module (import "baz" "quux") (import "foo" "bar" (memory 1 1)))'));
assertErrorMessage(() => new Instance(m4), TypeError, /no import object given/);
assertErrorMessage(() => new Instance(m4, {baz:null}), TypeError, /import object field is not an Object/);
assertErrorMessage(() => new Instance(m4, {baz:{quux:{}}}), TypeError, /import object field is not a Function/);
assertErrorMessage(() => new Instance(m4, {baz:{quux:()=>{}}, foo:null}), TypeError, /import object field is not an Object/);
assertErrorMessage(() => new Instance(m4, {baz:{quux:()=>{}}, foo:{bar:()=>{}}}), TypeError, /import object field is not a Memory/);
assertEq(new Instance(m3, {baz:{quux:()=>{}}, foo:{bar:mem1Page}}) instanceof Instance, true);

const m5 = new Module(textToBinary('(module (import "a" "b" (memory 2)))'));
assertErrorMessage(() => new Instance(m5, {a:{b:mem1Page}}), TypeError, /imported Memory with incompatible size/);
assertEq(new Instance(m5, {a:{b:mem2Page}}) instanceof Instance, true);
assertEq(new Instance(m5, {a:{b:mem3Page}}) instanceof Instance, true);
assertEq(new Instance(m5, {a:{b:mem4Page}}) instanceof Instance, true);

const m6 = new Module(textToBinary('(module (import "a" "b" (table 2)))'));
assertErrorMessage(() => new Instance(m6, {a:{b:tab1Elem}}), TypeError, /imported Table with incompatible size/);
assertEq(new Instance(m6, {a:{b:tab2Elem}}) instanceof Instance, true);
assertEq(new Instance(m6, {a:{b:tab3Elem}}) instanceof Instance, true);
assertEq(new Instance(m6, {a:{b:tab4Elem}}) instanceof Instance, true);

const m7 = new Module(textToBinary('(module (import "a" "b" (table 2 3)))'));
assertErrorMessage(() => new Instance(m7, {a:{b:tab1Elem}}), TypeError, /imported Table with incompatible size/);
assertEq(new Instance(m7, {a:{b:tab2Elem}}) instanceof Instance, true);
assertEq(new Instance(m7, {a:{b:tab3Elem}}) instanceof Instance, true);
assertErrorMessage(() => new Instance(m7, {a:{b:tab4Elem}}), TypeError, /imported Table with incompatible size/);

assertErrorMessage(() => new Module(textToBinary('(module (memory 2 1))')), TypeError, /maximum length less than initial length/);
assertErrorMessage(() => new Module(textToBinary('(module (import "a" "b" (memory 2 1)))')), TypeError, /maximum length less than initial length/);
assertErrorMessage(() => new Module(textToBinary('(module (table (resizable 2 1)))')), TypeError, /maximum length less than initial length/);
assertErrorMessage(() => new Module(textToBinary('(module (import "a" "b" (table 2 1)))')), TypeError, /maximum length less than initial length/);

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
        return { get bar() { arr.push("bar"); return new WebAssembly.Memory({initial:1}) } }
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

var code = textToBinary('(module)');
var e = new Instance(new Module(code)).exports;
assertEq(Object.keys(e).length, 0);

var code = textToBinary('(module (func) (export "foo" 0))');
var e = new Instance(new Module(code)).exports;
assertEq(Object.keys(e).join(), "foo");
assertEq(e.foo(), undefined);

var code = textToBinary('(module (func) (export "foo" 0) (export "bar" 0))');
var e = new Instance(new Module(code)).exports;
assertEq(Object.keys(e).join(), "foo,bar");
assertEq(e.foo(), undefined);
assertEq(e.bar(), undefined);
assertEq(e.foo, e.bar);

var code = textToBinary('(module (memory 1 1) (export "memory" memory))');
var e = new Instance(new Module(code)).exports;
assertEq(Object.keys(e).join(), "memory");

var code = textToBinary('(module (memory 1 1) (export "foo" memory) (export "bar" memory))');
var e = new Instance(new Module(code)).exports;
assertEq(Object.keys(e).join(), "foo,bar");
assertEq(e.foo, e.bar);
assertEq(e.foo instanceof Memory, true);
assertEq(e.foo.buffer.byteLength, 64*1024);

var code = textToBinary('(module (memory 1 1) (func) (export "foo" 0) (export "bar" memory))');
var e = new Instance(new Module(code)).exports;
assertEq(Object.keys(e).join(), "foo,bar");
assertEq(e.foo(), undefined);
assertEq(e.bar instanceof Memory, true);
assertEq(e.bar instanceof Memory, true);
assertEq(e.bar.buffer.byteLength, 64*1024);

var code = textToBinary('(module (memory 1 1) (func) (export "bar" memory) (export "foo" 0))');
var e = new Instance(new Module(code)).exports;
assertEq(Object.keys(e).join(), "bar,foo");
assertEq(e.foo(), undefined);
assertEq(e.bar.buffer.byteLength, 64*1024);

var code = textToBinary('(module (memory 1 1) (export "" memory))');
var e = new Instance(new Module(code)).exports;
assertEq(Object.keys(e).length, 1);
assertEq(String(Object.keys(e)), "");
assertEq(e[""] instanceof Memory, true);

var code = textToBinary('(module (table) (export "tbl" table))');
var e = new Instance(new Module(code)).exports;
assertEq(Object.keys(e).join(), "tbl");
assertEq(e.tbl instanceof Table, true);
assertEq(e.tbl.length, 0);

var code = textToBinary('(module (table (resizable 2)) (export "t1" table) (export "t2" table))');
var e = new Instance(new Module(code)).exports;
assertEq(Object.keys(e).join(), "t1,t2");
assertEq(e.t1 instanceof Table, true);
assertEq(e.t2 instanceof Table, true);
assertEq(e.t1, e.t2);
assertEq(e.t1.length, 2);

var code = textToBinary('(module (table (resizable 2)) (memory 1 1) (func) (export "t" table) (export "m" memory) (export "f" 0))');
var e = new Instance(new Module(code)).exports;
assertEq(Object.keys(e).join(), "t,m,f");
assertEq(e.f(), undefined);
assertEq(e.t instanceof Table, true);
assertEq(e.m instanceof Memory, true);
assertEq(e.t.length, 2);

var code = textToBinary('(module (table (resizable 1)) (memory 1 1) (func) (export "m" memory) (export "f" 0) (export "t" table))');
var e = new Instance(new Module(code)).exports;
assertEq(Object.keys(e).join(), "m,f,t");
assertEq(e.f(), undefined);
assertEq(e.t instanceof Table, true);
assertEq(e.m instanceof Memory, true);
+assertEq(e.t.length, 1);

var code = textToBinary('(module (table) (export "" table))');
var e = new Instance(new Module(code)).exports;
assertEq(Object.keys(e).length, 1);
assertEq(String(Object.keys(e)), "");
assertEq(e[""] instanceof Table, true);
+assertEq(e[""].length, 0);

// Table export function identity

var code = textToBinary(`(module
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
assertErrorMessage(() => e.tbl1.get(4), RangeError, /out-of-range index/);
assertEq(e.tbl1.get(1), null);
e.tbl1.set(1, e.f3);
assertEq(e.tbl1.get(1), e.f3);
e.tbl1.set(1, null);
assertEq(e.tbl1.get(1), null);
e.tbl1.set(3, e.f1);
assertEq(e.tbl1.get(0), e.tbl1.get(3));

// Re-exports:

var code = textToBinary('(module (import "a" "b" (memory 1 1)) (export "foo" memory) (export "bar" memory))');
var mem = new Memory({initial:1});
var e = new Instance(new Module(code), {a:{b:mem}}).exports;
assertEq(mem, e.foo);
assertEq(mem, e.bar);

var code = textToBinary('(module (import "a" "b" (table 1 1)) (export "foo" table) (export "bar" table))');
var tbl = new Table({initial:1});
var e = new Instance(new Module(code), {a:{b:tbl}}).exports;
assertEq(tbl, e.foo);
assertEq(tbl, e.bar);

// Non-existent export errors

assertErrorMessage(() => new Module(textToBinary('(module (export "a" 0))')), TypeError, /exported function index out of bounds/);
assertErrorMessage(() => new Module(textToBinary('(module (export "a" global 0))')), TypeError, /exported global index out of bounds/);
assertErrorMessage(() => new Module(textToBinary('(module (export "a" memory))')), TypeError, /exported memory index out of bounds/);
assertErrorMessage(() => new Module(textToBinary('(module (export "a" table))')), TypeError, /exported table index out of bounds/);

// Default memory/table rules

assertErrorMessage(() => new Module(textToBinary('(module (import "a" "b" (memory 1 1)) (memory 1 1))')), TypeError, /already have default memory/);
assertErrorMessage(() => new Module(textToBinary('(module (import "a" "b" (memory 1 1)) (import "x" "y" (memory 2 2)))')), TypeError, /already have default memory/);
assertErrorMessage(() => new Module(textToBinary('(module (import "a" "b" (table 1 1)) (table 1 1))')), TypeError, /already have default table/);
assertErrorMessage(() => new Module(textToBinary('(module (import "a" "b" (table 1 1)) (import "x" "y" (table 2 2)))')), TypeError, /already have default table/);

// Data segments on imports

var m = new Module(textToBinary(`
    (module
        (import "a" "b" (memory 1 1))
        (data 0 "\\0a\\0b")
        (data 100 "\\0c\\0d")
        (func $get (param $p i32) (result i32)
            (i32.load8_u (get_local $p)))
        (export "get" $get))
`));
var mem = new Memory({initial:1});
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

// Elem segments on imports

var m = new Module(textToBinary(`
    (module
        (import "a" "b" (table 10))
        (elem (i32.const 0) $one $two)
        (elem (i32.const 3) $three $four)
        (func $one (result i32) (i32.const 1))
        (func $two (result i32) (i32.const 2))
        (func $three (result i32) (i32.const 3))
        (func $four (result i32) (i32.const 4)))
`));
var tbl = new Table({initial:10});
new Instance(m, {a:{b:tbl}});
assertEq(tbl.get(0)(), 1);
assertEq(tbl.get(1)(), 2);
assertEq(tbl.get(2), null);
assertEq(tbl.get(3)(), 3);
assertEq(tbl.get(4)(), 4);
for (var i = 5; i < 10; i++)
    assertEq(tbl.get(i), null);
