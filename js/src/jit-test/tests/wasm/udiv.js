function test_i32_udiv_n2() {
    return wasmEvalText(
      `(module
         (type (;0;) (func (param i32) (result i32)))
         (func (;0;) (type 0) (param i32) (result i32)
           local.get 0
           i32.const -2
           i32.div_u)
         (export "main" (func 0)))`
    ).exports["main"];
}
function test_i32_udiv_n1() {
    return wasmEvalText(
      `(module
         (type (;0;) (func (param i32) (result i32)))
         (func (;0;) (type 0) (param i32) (result i32)
           local.get 0
           i32.const -1
           i32.div_u)
         (export "main" (func 0)))`
    ).exports["main"];
}
function test_i32_udiv_p2() {
    return wasmEvalText(
      `(module
         (type (;0;) (func (param i32) (result i32)))
         (func (;0;) (type 0) (param i32) (result i32)
           local.get 0
           i32.const 2
           i32.div_u)
         (export "main" (func 0)))`
    ).exports["main"];
}
function test_i32_udiv_p1() {
    return wasmEvalText(
      `(module
         (type (;0;) (func (param i32) (result i32)))
         (func (;0;) (type 0) (param i32) (result i32)
           local.get 0
           i32.const 1
           i32.div_u)
         (export "main" (func 0)))`
    ).exports["main"];
}

let udiv_p1 = test_i32_udiv_p1();
assertEq(udiv_p1(1), 1);
assertEq(udiv_p1(2), 2);
assertEq(udiv_p1(0x7fffffff), 0x7fffffff);
assertEq(udiv_p1(-1), -1); // results are converted to i32.

let udiv_p2 = test_i32_udiv_p2();
assertEq(udiv_p2(1), 0);
assertEq(udiv_p2(2), 1);
assertEq(udiv_p2(0x7fffffff), 0x7fffffff >> 1);
assertEq(udiv_p2(-1), 0x7fffffff); // -1 is converted to u32.

let udiv_n1 = test_i32_udiv_n1();
assertEq(udiv_n1(1), 0);
assertEq(udiv_n1(2), 0);
assertEq(udiv_n1(0x7fffffff), 0);
assertEq(udiv_n1(-1), 1);

let udiv_n2 = test_i32_udiv_n2();
assertEq(udiv_n2(1), 0);
assertEq(udiv_n2(2), 0);
assertEq(udiv_n2(0x7fffffff), 0);
assertEq(udiv_n2(-1), 1);
