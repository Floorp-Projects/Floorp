// |jit-test| --fast-warmup; --no-threads

function foo(phase, o1, o2) {
  switch (phase) {
    case 1:
      return o1.x;
    case 2:
      return o1.x + o2.x;
  }
}

// Set `foo` as last child and `bar` as last parent.
function phase1() {
  eval(`
  function bar(o) {
    foo(1, o);
  }
  with ({}) {}
  for (var j = 0; j < 100; j++) {
    var obj = {x: 1};
    obj["y" + (j % 10)] = 2;
    bar(obj);
  }
  bar({y: 1, x: 2});
  `);
}
phase1();

// Collect `bar`.
gc();

// Recompile `foo` monomorphically.
with ({}) {}
for (var i = 0; i < 100; i++) {
  foo(2, {x: 1}, {x: 1});
}

// Bail out and create a folded stub in `foo`.
// The child matches, so we use `bar` as the owning script.
for (var i = 0; i < 6; i++) {
  var obj = {x: 1};
  obj["y" + i] = 2;
  foo(2, {y: 1, x: 2}, obj);
}
