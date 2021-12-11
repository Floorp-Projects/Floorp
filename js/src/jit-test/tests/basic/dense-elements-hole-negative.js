// The index is negative before code generation.

let v = {};
let negativeIndex = -1;

function f(obj) {
  assertEq(obj[negativeIndex] === v, true);
}
for (let i = 0; i < 2000; i++) {
  let obj = {};
  obj[1] = {};
  obj[negativeIndex] = v;
  f(obj);
}

// The sign of the index changes after the code generation.

function g(obj, i) {
  for (let j = 0; j < 4; j++) {
    assertEq(obj[i-j] === v, true);
  }
}
for (let i = 0; i < 2000; i++) {
  let obj = {};
  obj[1] = {};
  let X = 2000 - i;
  for (let j = 0; j < 10; j++) {
    obj[X-j] = v;
  }
  g(obj, X);
}
