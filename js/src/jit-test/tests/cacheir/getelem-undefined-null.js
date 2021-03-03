function exists() {
  var a = {'null': 1, 'undefined': 2};
  for (var i = 0; i < 100; i++) {
    assertEq(a[null], 1);
    assertEq(a[undefined], 2);
  }
}

function missing() {
  var a = {};
  for (var i = 0; i < 100; i++) {
    assertEq(a[null], undefined);
    assertEq(a[undefined], undefined);
  }
}

function getter() {
  var a = {
    get null() {
      return 1;
    },
    get undefined() {
      return 2;
    }
  }
  for (var i = 0; i < 100; i++) {
    assertEq(a[null], 1);
    assertEq(a[undefined], 2);
  }
}

function primitive() {
  var v = true;
  for (var i = 0; i < 100; i++) {
    assertEq(v[null], undefined);
    assertEq(v[undefined], undefined);
  }
}

function mixed() {
  var a = {'null': 'null', 'undefined': 'undefined'};
  for (var i = 0; i < 100; i++) {
    var v = a[i % 2 ? null : undefined]
    assertEq(a[v], i % 2 ? 'null' : 'undefined');
  }
}

exists();
missing()
getter();
primitive();
mixed();
