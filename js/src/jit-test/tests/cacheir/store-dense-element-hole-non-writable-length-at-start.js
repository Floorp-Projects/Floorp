// Store an element into a previous hole value and later add more elements
// exceeding the initialised length. Cover both mono- and polymorphic call
// sites. The array has a non-writable length at the start.

function testStoreDenseHole() {
  var array = [/* hole */, /* hole */, /* hole */, /* hole */, ];
  Object.defineProperty(array, "length", {
      writable: false
  });

  function store(ar, index) {
    ar[index] = index;
  }

  for (var i = 0; i < 10; ++i) {
    store(array, i);
  }

  assertEq(array.length, 4);
  for (var i = 0; i < 4; ++i) {
    assertEq(array[i], i);
  }
  for (var i = 4; i < 10; ++i) {
    assertEq(i in array, false);
  }
}
testStoreDenseHole();

function testStoreDenseHolePoly() {
  var array = [/* hole */, /* hole */, /* hole */, /* hole */, ];
  Object.defineProperty(array, "length", {
    writable: false
  });

  function store(ar, index) {
    ar[index] = index;
  }

  var objects = [array, {}];

  for (var i = 0; i < 10; ++i) {
    for (var j = 0; j < objects.length; ++j) {
      store(objects[j], i);
    }
  }

  assertEq(array.length, 4);
  for (var i = 0; i < 4; ++i) {
    assertEq(array[i], i);
  }
  for (var i = 4; i < 10; ++i) {
    assertEq(i in array, false);
  }
}
testStoreDenseHolePoly();
