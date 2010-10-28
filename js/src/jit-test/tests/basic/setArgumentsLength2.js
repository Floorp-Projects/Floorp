// don't crash

var q;

function f() {
  while (arguments.length > 0) {
    q = arguments[arguments.length-1];
    arguments.length--;
  }
}

f(1, 2, 3, 4, 5);
