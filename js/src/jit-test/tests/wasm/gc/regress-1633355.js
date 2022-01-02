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
        (field $left (mut (ref null $wabbit)))
        (field $right (mut (ref null $wabbit)))
     ))
     (global $g (mut (ref null $wabbit)) (ref.null $wabbit))
     (func (export "init") (param $n i32)
       (global.set $g (call $make (local.get $n)))
     )
     (func $make (param $n i32) (result (ref null $wabbit))
       (local $tmp i32)
       (struct.new_with_rtt $wabbit (local.get $tmp) (ref.null $wabbit) (ref.null $wabbit) (rtt.canon $wabbit))
     )
`);
let mod = new WebAssembly.Module(bin);
let ins = new WebAssembly.Instance(mod).exports;

// Debugger can handle non-exposable fields, like (ref T).
ins.init(6)
