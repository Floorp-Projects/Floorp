var evalInFrame = (function(global) {
    var dbgGlobal = newGlobal();
    var dbg = new dbgGlobal.Debugger();
    dbg.addDebuggee(global);
})(this);
const Module = WebAssembly.Module;
const Instance = WebAssembly.Instance;
var m = new Module(wasmTextToBinary(`(module
    (table 3 anyfunc)
    (import $foo "" "foo" (result i32))
    (import $bar "" "bar" (result i32))
    (func $baz (result i32) (i32.const 13))
    (elem (i32.const 0) $foo $bar $baz)
    (export "tbl" table)
)`));
var jsFun = () => 83;
var wasmFun = new Instance(
  new Module(
    wasmTextToBinary('(module (func (result i32) (i32.const 42)) (export "foo" 0))')
  )
).exports.foo;
var e1 = new Instance(m, {
    "": {
        foo: jsFun,
        bar: wasmFun
    }
}).exports;
var e2 = new Instance(m, {
    "": {
        foo: jsFun,
        bar: jsFun
    }
}).exports;
e2.tbl.get(0);
