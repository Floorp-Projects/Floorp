function str(c) {
  let s = c;
  for (let i = 0; i < 30; i++) {
    s += c;
  }
  ensureLinearString(s);
  return s;
}

function f() {
  // Create some slots to write into.
  const o = { owner: "short1", s: "short2" };

  // Make a tenured rope.
  const r1 = str("a") + str("b");
  gc();

  // Write the first instance of our duplicate string into one of the slots
  // (`owner`). This will be scanned first, and entered into deDupSet when
  // tenured.
  o.owner = ensureLinearString(str("a") + str("b") + str("c"));

  // Make another rope with identical contents, with a tenured subtree.
  const r2 = r1 + str("c");

  // Linearize the new rope, creating a new extensible string and a bunch of
  // dependent strings replacing the rest of the rope nodes.
  ensureLinearString(r2);

  // Write the new rope into a slot, so that it will be scanned next during the
  // minor GC during traceSlots().
  o.s = r2;

  // Do a nursery collection. o.owner will be tenured and inserted into
  // deDupSet. Then o.s aka r2 will be tenured. If things work correctly, r2
  // will be marked non-deduplicatable because it is the base of a tenured
  // string r1. If not, it will be deduplicated to o.owner.
  minorgc();

  // Extract out that r1 child node. If its base was deduplicated, this will
  // assert because its chars have been freed.
  const s1 = r1.substr(0, 31);
}

f();
