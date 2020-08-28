// Store an element into a previous hole value and later add more elements
// exceeding the initialised length. Cover both mono- and polymorphic call
// sites.

function testStoreDenseHole() {
  var array = [/* hole */, /* hole */, /* hole */, /* hole */, ];

  function store(ar, index) {
    ar[index] = index;
  }

  for (var i = 0; i < 10; ++i) {
    store(array, i);
  }

  assertEq(array.length, 10);
  for (var i = 0; i < 10; ++i) {
    assertEq(array[i], i);
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
    for (var j = 0; j < objects.length; ++j) {
      store(objects[j], i);
    }
  }

  assertEq(array.length, 10);
  for (var i = 0; i < 10; ++i) {
    assertEq(array[i], i);
  }
}
testStoreDenseHolePoly();
