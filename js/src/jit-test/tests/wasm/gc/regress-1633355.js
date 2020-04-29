// |jit-test| skip-if: !wasmGcEnabled()
load(libdir + "asserts.js");

var g23 = newGlobal({newCompartment: true});
g23.parent = this;
g23.eval(`
    var dbg = new Debugger(parent);
    dbg.onEnterFrame = function(frame) {}
`);
let bin = wasmTextToBinary(`
     (type $wabbit (struct
        (field $x (mut i32))
        (field $left (mut (ref opt $wabbit)))
        (field $right (mut (ref opt $wabbit)))
     ))
     (global $g (mut (ref opt $wabbit)) (ref.null))
     (func (export "init") (param $n i32)
       (global.set $g (call $make (local.get $n)))
     )
     (func $make (param $n i32) (result (ref opt $wabbit))
       (local $tmp i32)
       (struct.new $wabbit (local.get $tmp) (ref.null) (ref.null))
     )
`);
let mod = new WebAssembly.Module(bin);
let ins = new WebAssembly.Instance(mod).exports;

assertErrorMessage(() => ins.init(6), TypeError,
                    /conversion from WebAssembly typed ref to JavaScript/);
