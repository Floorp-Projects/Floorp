function assertThrowsReferenceError(f) {
  var err;
  try {
    f();
  } catch (e) {
    err = e;
  }
  assertEq(err instanceof ReferenceError, true);
}

function f() {
    switch (0) {
        case 1:
            let x
        case function() {
            print(x)
        }():
    }
}
assertThrowsReferenceError(f);

function g() {
  switch (0) {
    case 1:
      let x;
    case 0:
      var inner = function () {
        print(x);
      }
      inner();
      break;
  }
}
assertThrowsReferenceError(g);

function h() {
  switch (0) {
    case 0:
      var inner = function () {
        print(x);
      }
      inner();
    case 1:
      let x;
  }
}
assertThrowsReferenceError(h);

// Tests that a dominating lexical doesn't throw.
function F() {
  switch (0) {
    case 0:
      let x = 42;
      var inner = function () {
        assertEq(x, 42);
      }
      inner();
  }
}
F();
