
function f() {}
function g(a,b) {}
function h(me, too, here) { var x = 1; }
function annoying(a, b, a, b, b, a) {}
function manyLocals(a, b, c, d, e, f, g, h, i, j, k, l, m) {
  var n, o, p, q, r, s, t, u, v, w, x, y, z;
}

assertArraysEqual(jsd.wrapValue(f).script.getParameterNames(), []);
assertArraysEqual(jsd.wrapValue(g).script.getParameterNames(), ["a", "b"]);
assertArraysEqual(jsd.wrapValue(h).script.getParameterNames(), ["me", "too", "here"]);
assertArraysEqual(jsd.wrapValue(annoying).script.getParameterNames(),
                  ["a", "b", "a", "b", "b", "a"]);
assertArraysEqual(jsd.wrapValue(manyLocals).script.getParameterNames(),
                  "abcdefghijklm".split(""));

if (!jsdOnAtStart) {
  // turn JSD off if it wasn't on when this test started
  jsd.off();
  ok(!jsd.isOn, "JSD shouldn't be running at the end of this test.");
}

SimpleTest.finish();