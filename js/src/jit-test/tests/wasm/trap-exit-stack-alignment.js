// Function with high register pressure to force spills without function calls
// even on ARM64, and with a trap exit.
let func = wasmEvalText(`(module
    (func
        (param i32)
        (result i32)
        (local i32)
        (local i32)
        (local i32)
        (local i32)
        (local i32)
        (local i32)
        (local i32)
        (local i32)
        (local i32)
        (local i32)
        (local i32)
        (local i32)
        (local i32)
        (local i32)
        (local i32)
        (local i32)
        (local i32)
        (local i32)
        (local i32)
        (local i32)
        (local i32)
        (local i32)
        (local i32)
        (local i32)
        (local i32)
        (local i32)
        (local.set 2 (i32.add (local.get 0 i32.const 4)))
        (local.set 3 (i32.add (local.get 0 i32.const 5)))
        (local.set 4 (i32.add (local.get 0 i32.const 6)))
        (local.set 5 (i32.add (local.get 0 i32.const 7)))
        (local.set 6 (i32.add (local.get 0 i32.const 8)))
        (local.set 7 (i32.add (local.get 0 i32.const 9)))
        (local.set 8 (i32.add (local.get 0 i32.const 10)))
        (local.set 9 (i32.add (local.get 0 i32.const 11)))
        (local.set 10 (i32.add (local.get 0 i32.const 12)))
        (local.set 11 (i32.add (local.get 0 i32.const 13)))
        (local.set 12 (i32.add (local.get 0 i32.const 14)))
        (local.set 13 (i32.add (local.get 0 i32.const 15)))
        (local.set 14 (i32.add (local.get 0 i32.const 16)))
        (local.set 15 (i32.add (local.get 0 i32.const 17)))
        (local.set 16 (i32.add (local.get 0 i32.const 18)))
        (local.set 17 (i32.add (local.get 0 i32.const 19)))
        (local.set 18 (i32.add (local.get 0 i32.const 20)))
        (local.set 19 (i32.add (local.get 0 i32.const 21)))
        (local.set 20 (i32.add (local.get 0 i32.const 22)))
        (local.set 21 (i32.add (local.get 0 i32.const 23)))
        (local.set 22 (i32.add (local.get 0 i32.const 24)))
        (local.set 23 (i32.add (local.get 0 i32.const 25)))
        (local.set 24 (i32.add (local.get 0 i32.const 26)))
        (local.set 25 (i32.add (local.get 0 i32.const 27)))
        (local.set 26 (i32.add (local.get 0 i32.const 28)))
        (local.set 7 (i32.div_s (local.get 0) (local.get 0)))
        local.get 0
        local.get 1
        local.get 2
        local.get 3
        local.get 4
        local.get 5
        local.get 6
        local.get 7
        local.get 8
        local.get 9
        local.get 10
        local.get 11
        local.get 12
        local.get 13
        local.get 14
        local.get 15
        local.get 16
        local.get 17
        local.get 18
        local.get 19
        local.get 20
        local.get 21
        local.get 22
        local.get 23
        local.get 24
        local.get 25
        local.get 26
        i32.add
        i32.add
        i32.add
        i32.add
        i32.add
        i32.add
        i32.add
        i32.add
        i32.add
        i32.add
        i32.add
        i32.add
        i32.add
        i32.add
        i32.add
        i32.add
        i32.add
        i32.add
        i32.add
        i32.add
        i32.add
        i32.add
        i32.add
        i32.add
        i32.add
        i32.add
    )
    (export "" (func 0))
)`).exports[""];
assertEq(func(1), 417);
assertEq(func(2), 442);
let ex;
try {
  func(0);
} catch (e) {
  ex = e;
}
assertEq(ex instanceof WebAssembly.RuntimeError, true);
