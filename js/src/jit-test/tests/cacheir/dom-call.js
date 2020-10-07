function simple() {
  var obj = new FakeDOMObject();
  for (var i = 0; i < 10; i++) {
    assertEq(obj.doFoo(0, 0, 0), 3);
  }
}

function wrongThis() {
  var obj = new FakeDOMObject();
  var wrong = {doFoo: obj.doFoo};

  for (var i = 0; i < 100; i++) {
    assertEq(obj.doFoo(0, 0), i <= 50 ? 2 : undefined);
    if (i == 50) {
      obj = wrong;
    }
  }
}

function spread() {
 var obj = new FakeDOMObject();
  for (var i = 0; i < 10; i++) {
    assertEq(obj.doFoo(...[1, 2, 3, 4]), 4);
  }
}

simple();
wrongThis();
spread();