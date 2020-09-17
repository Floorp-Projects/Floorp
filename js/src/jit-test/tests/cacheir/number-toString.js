function int32() {
  var n = 42;
  for (var i = 0; i < 100; i++) {
    assertEq(n.toString(), "42");
  }
}

function double() {
  var n = 3.14;
  for (var i = 0; i < 100; i++) {
    assertEq(n.toString(), "3.14");
  }
}

function number() {
  var n = 1;
  for (var i = 0; i < 100; i++) {
    assertEq(n.toString(), i > 50 ? "3.14" : "1");
    if (i == 50) {
      n = 3.14;
    }
  }
}

function obj() {
  var o = new Number(42);
  for (var i = 0; i < 100; i++) {
    assertEq(o.toString(), "42");
  }
}

function overwritten() {
  Number.prototype.toString = () => "haha";
  var n = 42;
  for (var i = 0; i < 100; i++) {
    assertEq(n.toString(), "haha");
  }
}

int32();
double();
number();
obj();
overwritten();
