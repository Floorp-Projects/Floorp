// Add dense elements to packed and non-packed arrays. Cover both mono- and
// polymorphic call sites. Change array to non-extensible during execution.

function testAddDenseEmpty() {
  var array = [];

  function store(ar, index) {
    ar[index] = index;
  }

  for (var i = 0; i < 10; ++i) {
    if (i === 5) {
      Object.preventExtensions(array);
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
testAddDenseEmpty();

function testAddDensePacked() {
  var array = [0, 1];

  function store(ar, index) {
    ar[index] = index;
  }

  for (var i = 2; i < 10; ++i) {
    if (i === 5) {
      Object.preventExtensions(array);
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
testAddDensePacked();

function testAddDenseNonPacked() {
  var array = [/* hole */, 1];

  function store(ar, index) {
    ar[index] = index;
  }

  for (var i = 2; i < 10; ++i) {
    if (i === 5) {
      Object.preventExtensions(array);
    }
    store(array, i);
  }

  assertEq(array.length, 5);
  assertEq(0 in array, false);
  for (var i = 1; i < 5; ++i) {
    assertEq(array[i], i);
  }
  for (var i = 5; i < 10; ++i) {
    assertEq(i in array, false);
  }
}
testAddDenseNonPacked();

function testAddDenseEmptyPoly() {
  var array = [];

  function store(ar, index) {
    ar[index] = index;
  }

  var objects = [array, {}];

  for (var i = 0; i < 10; ++i) {
    if (i === 5) {
      Object.preventExtensions(array);
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
testAddDenseEmptyPoly();

function testAddDensePackedPoly() {
  var array = [0, 1];

  function store(ar, index) {
    ar[index] = index;
  }

  var objects = [array, {}];

  for (var i = 2; i < 10; ++i) {
    if (i === 5) {
      Object.preventExtensions(array);
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
testAddDensePackedPoly();

function testAddDenseNonPackedPoly() {
  var array = [/* hole */, 1];

  function store(ar, index) {
    ar[index] = index;
  }

  var objects = [array, {}];

  for (var i = 2; i < 10; ++i) {
    if (i === 5) {
      Object.preventExtensions(array);
    }
    for (var j = 0; j < objects.length; ++j) {
      store(objects[j], i);
    }
  }

  assertEq(array.length, 5);
  assertEq(0 in array, false);
  for (var i = 1; i < 5; ++i) {
    assertEq(array[i], i);
  }
  for (var i = 5; i < 10; ++i) {
    assertEq(i in array, false);
  }
}
testAddDenseNonPackedPoly();
