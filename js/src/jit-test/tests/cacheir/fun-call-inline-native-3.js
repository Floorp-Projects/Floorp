// Test inlining natives through Function.prototype.call
//
// Array() is inlined when there are 0-1 arguments.

function arrayThisAbsent() {
  for (let i = 0; i < 400; ++i) {
    let r = Array.call();
    assertEq(r.length, 0);
  }
}
for (let i = 0; i < 2; ++i) arrayThisAbsent();

function array0() {
  for (let i = 0; i < 400; ++i) {
    let r = Array.call(null);
    assertEq(r.length, 0);
  }
}
for (let i = 0; i < 2; ++i) array0();

function array1() {
  for (let i = 0; i < 400; ++i) {
    let r = Array.call(null, 5);
    assertEq(r.length, 5);
  }
}
for (let i = 0; i < 2; ++i) array1();

function array2() {
  for (let i = 0; i < 400; ++i) {
    let r = Array.call(null, 5, 10);
    assertEq(r.length, 2);
  }
}
for (let i = 0; i < 2; ++i) array2();
