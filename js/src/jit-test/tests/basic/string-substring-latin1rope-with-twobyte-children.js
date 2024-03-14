let right = newRope("b", "012345678901234567890123456789");
let latin1Rope = newRope("a", right);
let twoByteRope = newRope("\u221e", right);

// Flattening |twoByteRope| changes |right| from a Latin-1 rope into a two-byte
// dependent string. At this point, |latin1Rope| has the Latin-1 flag set, but
// also has a two-byte rope child.
ensureLinearString(twoByteRope);

let result = latin1Rope.substring(0, 3);

assertEq(result, "ab0");
