// Bug 1488515 - ABI issues on Android because both the baseline compiler and
// the builtin thunks converted between hardFP and softFP.

var prog = wasmEvalText(
    `(module
       (func $f64div (export "test") (param $a f64) (param $b f64) (result f64)
         (local $dummy0 i32)
         (local $dummy1 i32)
         (local $dummy2 i32)
         (local $dummy3 i32)
         (local $dummy4 i32)
         (local $x f64)
         (local $y f64)
         (local $z f64)
         (local.set $x (local.get $a))
         (local.set $y (local.get $b))
         (local.set $z (f64.floor (f64.div (local.get $x) (local.get $y))))
         (local.get $z)))`);

assertEq(prog.exports.test(16096, 32), 503);
