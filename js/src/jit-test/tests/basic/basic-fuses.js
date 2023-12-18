// |jit-test| skip-if: !(getBuildConfiguration('debug')||getBuildConfiguration('fuzzing-defined'))

function intact(name) {
  let state = getFuseState();
  if (!(name in state)) {
    throw "No such fuse " + name;
  }
  return state[name].intact
}

g = newGlobal();
g.evaluate(intact.toString())

g.evaluate("delete Array.prototype[Symbol.iterator]")
g.evaluate(`assertEq(intact("ArrayPrototypeIteratorFuse"), false)`)

function testRealmChange() {
  let g = newGlobal();
  g.evaluate(intact.toString())

  // Get a mutating function which will affect the symbol.iterator fuse.
  let rdel = g.evaluate("function del(o) { delete o.prototype[Symbol.iterator] };del")
  // Fuse is still intact.
  g.evaluate(`assertEq(intact("ArrayPrototypeIteratorFuse"), true)`);

  // setup a new global,
  let g2 = newGlobal();
  g2.evaluate(intact.toString())

  // register the popping function.
  g2.rdel = rdel;

  // Pop the array fuse in the new global.
  g2.evaluate(`rdel(Array)`);

  // The realm of the original array should have a fuse still intact
  g.evaluate(`assertEq(intact("ArrayPrototypeIteratorFuse"), true)`);

  // The realm of the array proto should no longer be intact. Oh dear. This is
  // interesting. We currently ask the cx for the array iterator proto,
  g2.evaluate(`assertEq(intact("ArrayPrototypeIteratorFuse"), false)`);
}

testRealmChange();
