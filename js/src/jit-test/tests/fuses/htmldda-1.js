function f(x, y, z, a, b, c) {
  let sum = 0;
  sum += x == undefined ? 1 : 0;
  sum += y == undefined ? 1 : 0;
  sum += z == undefined ? 1 : 0;
  sum += a == undefined ? 1 : 0;
  sum += b == undefined ? 1 : 0;
  sum += c == undefined ? 1 : 0;
  return sum;
}

let iters = 500;
function test(x) {
  let count = 0;
  let [y, z, a, b, c] = [{}, {}, {}, {}, {}];
  for (let i = 0; i < iters; i++) {
    count += f(x, y, z, a, b, c) ? 1 : 0;
  }
  return count;
}

let count = test({});
assertEq(count, 0);

// pop fuse, and run test again.
x = createIsHTMLDDA();
count = test(x);

assertEq(count, iters);
