function test() {
  var table = wasmEvalText(`(module
        (func $add0 (param i32) (result i32) (i32.add (local.get 0) (i32.const 0)))
        (func $add1 (param i32) (result i32) (i32.add (local.get 0) (i32.const 1)))
        (func $add2 (param i32) (result i32) (i32.add (local.get 0) (i32.const 2)))
        (func $add3 (param i32) (result i32) (i32.add (local.get 0) (i32.const 3)))
        (func $add4 (param i32) (result i32) (i32.add (local.get 0) (i32.const 4)))
        (func $add5 (param i32) (result i32) (i32.add (local.get 0) (i32.const 5)))
        (func $add6 (param i32) (result i32) (i32.add (local.get 0) (i32.const 6)))
        (func $add7 (param i32) (result i32) (i32.add (local.get 0) (i32.const 7)))
        (func $add8 (param i32) (result i32) (i32.add (local.get 0) (i32.const 8)))
        (func $add9 (param i32) (result i32) (i32.add (local.get 0) (i32.const 9)))
        (table (export "table") 10 funcref)
        (elem (i32.const 0) $add0)
        (elem (i32.const 1) $add1)
        (elem (i32.const 2) $add2)
        (elem (i32.const 3) $add3)
        (elem (i32.const 4) $add4)
        (elem (i32.const 5) $add5)
        (elem (i32.const 6) $add6)
        (elem (i32.const 7) $add7)
        (elem (i32.const 8) $add8)
        (elem (i32.const 9) $add9)
    )`).exports.table;
  var exps = [];
  for (var i = 0; i < 10; i++) {
    exps.push(table.get(i));
  }
  var res = 0;
  for (var i = 0; i < 80; i++) {
    var exp = exps[i % exps.length];
    res = exp(res);
  }
  assertEq(res, 360);
}
test();
