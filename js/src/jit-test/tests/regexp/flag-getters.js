// Test inlining for RegExp flag getters.

function testGlobal() {
  const xs = [/a/, /b/g];

  for (let i = 0; i < 200; ++i) {
    let x = xs[i & 1];
    assertEq(x.global, !!(i & 1));
  }
}
for (let i = 0; i < 2; ++i) testGlobal();

function testIgnoreCase() {
  const xs = [/a/, /b/i];

  for (let i = 0; i < 200; ++i) {
    let x = xs[i & 1];
    assertEq(x.ignoreCase, !!(i & 1));
  }
}
for (let i = 0; i < 2; ++i) testIgnoreCase();

function testMultiline() {
  const xs = [/a/, /b/m];

  for (let i = 0; i < 200; ++i) {
    let x = xs[i & 1];
    assertEq(x.multiline, !!(i & 1));
  }
}
for (let i = 0; i < 2; ++i) testMultiline();

function testDotAll() {
  const xs = [/a/, /b/s];

  for (let i = 0; i < 200; ++i) {
    let x = xs[i & 1];
    assertEq(x.dotAll, !!(i & 1));
  }
}
for (let i = 0; i < 2; ++i) testDotAll();

function testUnicode() {
  const xs = [/a/, /b/u];

  for (let i = 0; i < 200; ++i) {
    let x = xs[i & 1];
    assertEq(x.unicode, !!(i & 1));
  }
}
for (let i = 0; i < 2; ++i) testUnicode();

function testSticky() {
  const xs = [/a/, /b/y];

  for (let i = 0; i < 200; ++i) {
    let x = xs[i & 1];
    assertEq(x.sticky, !!(i & 1));
  }
}
for (let i = 0; i < 2; ++i) testSticky();
