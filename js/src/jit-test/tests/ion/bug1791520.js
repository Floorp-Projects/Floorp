function testAtomicsAdd() {
  var x;
  for (var i = 0; i < 100; ++i) {
    var a = new BigInt64Array(2);
    x = Atomics.add(a, i & 1, 1n);
  }
  return x;
}

function testAtomicsSub() {
  var x;
  for (var i = 0; i < 100; ++i) {
    var a = new BigInt64Array(2);
    x = Atomics.sub(a, i & 1, 1n);
  }
  return x;
}

function testAtomicsAnd() {
  var x;
  for (var i = 0; i < 100; ++i) {
    var a = new BigInt64Array(2);
    x = Atomics.and(a, i & 1, 1n);
  }
  return x;
}

function testAtomicsOr() {
  var x;
  for (var i = 0; i < 100; ++i) {
    var a = new BigInt64Array(2);
    x = Atomics.or(a, i & 1, 1n);
  }
  return x;
}

function testAtomicsXor() {
  var x;
  for (var i = 0; i < 100; ++i) {
    var a = new BigInt64Array(2);
    x = Atomics.xor(a, i & 1, 1n);
  }
  return x;
}

function testAtomicsExchange() {
  var x;
  for (var i = 0; i < 100; ++i) {
    var a = new BigInt64Array(2);
    x = Atomics.exchange(a, i & 1, 0n);
  }
  return x;
}

function testAtomicsCompareExchange() {
  var x;
  for (var i = 0; i < 100; ++i) {
    var a = new BigInt64Array(2);
    x = Atomics.compareExchange(a, i & 1, 0n, 0n);
  }
  return x;
}

function testAtomicsLoad() {
  var x;
  for (var i = 0; i < 100; ++i) {
    var a = new BigInt64Array(2);
    x = Atomics.load(a, i & 1);
  }
  return x;
}

function testLoadElement() {
  var x;
  for (var i = 0; i < 100; ++i) {
    var a = new BigInt64Array(2);
    x = a[i & 1];
  }
  return x;
}

gczeal(14);

testAtomicsAdd();
testAtomicsSub();
testAtomicsAnd();
testAtomicsOr();
testAtomicsXor();
testAtomicsExchange();
testAtomicsCompareExchange();
testAtomicsLoad();
testLoadElement();
