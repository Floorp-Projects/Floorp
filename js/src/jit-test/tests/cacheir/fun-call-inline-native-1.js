// Test inlining natives through Function.prototype.call
//
// Math.min() is inlined when there are 1-4 arguments.

function mathMinThisAbsent() {
  for (let i = 0; i < 400; ++i) {
    let r = Math.min.call();
    assertEq(r, Infinity);
  }
}
for (let i = 0; i < 2; ++i) mathMinThisAbsent();

function mathMin0() {
  for (let i = 0; i < 400; ++i) {
    let r = Math.min.call(null);
    assertEq(r, Infinity);
  }
}
for (let i = 0; i < 2; ++i) mathMin0();

function mathMin1() {
  for (let i = 0; i < 400; ++i) {
    let r = Math.min.call(null, i);
    assertEq(r, i);
  }
}
for (let i = 0; i < 2; ++i) mathMin1();

function mathMin2() {
  for (let i = 0; i < 400; ++i) {
    let r = Math.min.call(null, i, i + 1);
    assertEq(r, i);
  }
}
for (let i = 0; i < 2; ++i) mathMin2();

function mathMin3() {
  for (let i = 0; i < 400; ++i) {
    let r = Math.min.call(null, i, i + 1, i + 2);
    assertEq(r, i);
  }
}
for (let i = 0; i < 2; ++i) mathMin3();

function mathMin4() {
  for (let i = 0; i < 400; ++i) {
    let r = Math.min.call(null, i, i + 1, i + 2, i + 3);
    assertEq(r, i);
  }
}
for (let i = 0; i < 2; ++i) mathMin4();

function mathMin5() {
  for (let i = 0; i < 400; ++i) {
    let r = Math.min.call(null, i, i + 1, i + 2, i + 3, i + 4);
    assertEq(r, i);
  }
}
for (let i = 0; i < 2; ++i) mathMin5();
