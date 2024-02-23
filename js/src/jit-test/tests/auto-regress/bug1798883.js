// String with an initial part which doesn't need to be normalised and a tail
// which gets normalised to "\u05E9\u05BC\u05C1".
var s = "a".repeat(32) + String.fromCharCode(0xFB2C);

oomTest(function() {
  // |normalize()| needs to be called at least twice to trigger the bug.
  s.normalize();
  s.normalize();
});
