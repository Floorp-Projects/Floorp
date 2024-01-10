// `str.charCodeAt(0)` doesn't need to inspect the right rope child, because
// the first character is guaranteed to be in the left child.

const ropes = [
  newRope("ABCDEFGHIJKL", "MNOPQRSTUVWXYZ"),
  newRope("abcdefghijkl", "mnopqrstuvwxyz"),

  newRope("A", "BCDEFGHIJKLMNOPQRSTUVWXYZ"),
  newRope("a", "bcdefghijklmnopqrstuvwxyz"),
];

for (let i = 0; i < 500; ++i) {
  let rope = ropes[i & 3];

  let actual = rope.charCodeAt(0);
  let expected = 0x41 + (i & 1) * 0x20;
  assertEq(actual, expected);
}
