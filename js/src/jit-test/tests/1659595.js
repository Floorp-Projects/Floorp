
// Impliclitly converting regular numeric properties to computed numeric
// properties in the parser means checking there's no semantic changes.

let a = {
  0: 0,
  1n: 1n,

  get 2() {
    return 2;
  },
  set 2(o) {},

  get 3n() {
    return 3n;
  },
  set 3n(o) {}
};

assertEq(a[0], 0);
assertEq(a[1n], 1n);
assertEq(a[2], 2);
assertEq(a[3n], 3n);

function getterName(x) {
  return Object.getOwnPropertyDescriptor(a, x).get.name
}
function setterName(x) {
  return Object.getOwnPropertyDescriptor(a, x).set.name
}

assertEq(/get 2/.test(getterName(2)), true)
assertEq(/get 3/.test(getterName(3n)), true)

assertEq(/set 2/.test(setterName(2)), true)
assertEq(/set 3/.test(setterName(3n)), true)

let b = {
  0: 0,
  [0]: 'dupe',
  1n: 1,
  [1n]: 'dupe',
  [2]: 2,
  2: 'dupe',
  [3n]: 3,
  3n: 'dupe'
};
assertEq(b[0], 'dupe');
assertEq(b[1n], 'dupe');
assertEq(b[2], 'dupe');
assertEq(b[3n], 'dupe');

let c = {
  0: 0,
  0.0: 'dupe',
};
assertEq(c[0], 'dupe');

let d = {
  0: 0,
  '0': 'dupe',
};
assertEq(d[0], 'dupe');

// Test numeric property destructuring.
let {1: x} = {1: 1};
let {1n: y} = {1n: 1n};

assertEq(x, 1);
assertEq(y, 1n);
