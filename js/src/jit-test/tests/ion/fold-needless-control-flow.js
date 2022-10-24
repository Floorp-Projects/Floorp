function test(str, needle) {
  // We should be able to fold away the complete indexOf instruction, because
  // executing the right-hand side of the && operator is a no-op.
  (str.indexOf(needle) > -1) && needle;
}

const needles = [
  "a", "b",
];

for (let i = 0; i < 100; ++i) {
  test("aaa", needles[i & 1]);
}
