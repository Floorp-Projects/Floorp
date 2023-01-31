let toBeIncremented = 0;
function megamorphicGetIncremented(thisIsMegamorphic, thisIsAlwaysTrue) {
  // We need this to always evaluate as foo, and have an else clause which
  // would bail if we ever hit it.
  let key = thisIsAlwaysTrue ? "foo" : thisIsMegamorphic.bob;

  // The first megamorphic load:
  if (!thisIsMegamorphic[key]) {
    // The store which invalidates it
    thisIsMegamorphic[key] = ++toBeIncremented;
  }
  // The megamorphic load which was bugged
  return thisIsMegamorphic[key];
}

// We just need enough shapes to go megamorphic. Put in a bunch though
// just to be sure
let objShapes = [
  {a: 1},
  // We need the shapes to occasionally have "foo" defined, but be false,
  // otherwise stub folding will mean we don't go megamorphic because
  // we'll just attach "Missing" which in our case just differs by a
  // single shape guard.
  {b: 1, baz: 2, foo: false},
  {c: 1},
  {d: 1, baz: 2, foo: false},
  {e: 1},
  {f: 1, baz: 2, foo: false},
  {g: 1},
  {h: 1, baz: 2, foo: false},
  {i: 1},
  {j: 1, baz: 2, foo: false},
  {k: 1},
  {l: 1, baz: 2, foo: false},
  {m: 1},
  {n: 1, baz: 2, foo: false},
  {o: 1},
  {p: 1, baz: 2, foo: false},
  {q: 1},
  {r: 1, baz: 2, foo: false},
  {s: 1},
  {t: 1, baz: 2, foo: false},
];
let objs = [];
for (let i = 0; i < 100; i++) {
  let obj = Object.assign({}, objShapes[i % objShapes.length]);
  objs.push(obj);
}

for (let i = 1; i < 100; i++) {
  let id = megamorphicGetIncremented(objs[i], true);
  assertEq(id, i);
}
