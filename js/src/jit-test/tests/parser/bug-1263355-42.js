// |jit-test| error: ReferenceError

function f() {
  switch (2) {
    case 1:
      x = 1;
    case (x, 2):
      let x;
  }
}
f();
