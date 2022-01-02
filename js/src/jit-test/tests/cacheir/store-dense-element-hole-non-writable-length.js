// Store an element into a previous hole value and later add more elements
// exceeding the initialised length. Cover both mono- and polymorphic call
// sites. Change array length to non-writable during execution.

function testStoreDenseHole() {
  var array = [/* hole */, /* hole */, /* hole */, /* hole */, ];

  function store(ar, index) {
    ar[index] = index;
  }

  for (var i = 0; i < 10; ++i) {
    if (i === 5) {
      Object.defineProperty(array, "length", {writable: false});
    }
    store(array, i);
  }

  assertEq(array.length, 5);
  for (var i = 0; i < 5; ++i) {
    assertEq(array[i], i);
  }
  for (var i = 5; i < 10; ++i) {
    assertEq(i in array, false);
  }
}
testStoreDenseHole();

function testStoreDenseHolePoly() {
  var array = [/* hole */, /* hole */, /* hole */, /* hole */, ];

  function store(ar, index) {
    ar[index] = index;
  }

  var objects = [array, {}];

  for (var i = 0; i < 10; ++i) {
    if (i === 5) {
      Object.defineProperty(array, "length", {writable: false});
    }
    for (var j = 0; j < objects.length; ++j) {
      store(objects[j], i);
    }
  }

  assertEq(array.length, 5);
  for (var i = 0; i < 5; ++i) {
    assertEq(array[i], i);
  }
  for (var i = 5; i < 10; ++i) {
    assertEq(i in array, false);
  }
}
testStoreDenseHolePoly();
