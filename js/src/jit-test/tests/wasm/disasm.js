// |jit-test| skip-if: !hasDisassembler()

// Test that the disassembler is reasonably sane.

var mod = new WebAssembly.Module(wasmTextToBinary(`
(module
   (func $hum (import "m" "hum") (param i32) (result f64))
   (memory 1)
   (func $hi (export "f") (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (result i32)
     (i32.add (i32.load (local.get 5)) (i32.load (local.get 6))))
   (func $ho (param i32) (result i32) (i32.const 37))
)
`));

// The following capture the disassembly as a string.  We can't really check
// that no other output is produced.

var s = wasmDis(mod, {tier:'best', asString:true});
assertEq(typeof s, "string")
assertEq(s.match(/Kind = Function/g).length, 3)

var ins = new WebAssembly.Instance(mod, {m:{hum:(x) => x+0.5}});
var s = wasmDis(ins, {tier:'best', asString:true});
assertEq(typeof s, "string")
assertEq(s.match(/Kind = Function/g).length, 3)

var s = wasmDis(ins.exports.f, {tier:'best', asString:true})
assertEq(typeof s, "string")

var s = wasmDis(ins, {asString:true, kinds:"InterpEntry,ImportInterpExit,Function"})
assertEq(typeof s, "string")
assertEq(s.match(/Kind = Function/g).length, 3)
assertEq(s.match(/Kind = InterpEntry/g).length, 1)
assertEq(s.match(/Kind = ImportInterpExit/g).length, 1)
assertEq(s.match(/name = hi/g).length, 2)
assertEq(s.match(/name = ho/g).length, 1)
assertEq(s.match(/name = hum/g).length, 2)

// This one prints to stderr, we can't check the output but we can check that a
// string is not returned.

var s = wasmDis(ins, {tier:'best'})
assertEq(typeof s, "undefined")
