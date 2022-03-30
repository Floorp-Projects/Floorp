let throwing = false;

function bar() {
  with ({}) {}
  if (throwing) throw 3;
}

function foo() {
  let y = 3;
  try {
    let x = 3;
    () => { return x + y; }
    bar();
  } finally {
    assertEq(y, 3);
  }
}

with ({}) {}

for (var i = 0; i < 1000; i++) {
  foo()
}

throwing = true;
try {
  foo();
} catch {}
