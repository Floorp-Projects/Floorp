function test() {
  {
    assertEq(f(), 2);
    function f() { return 1; }
    assertEq(f(), 2);
    function f() { return 2; }
    assertEq(f(), 2);
  }

  // Annex B still works.
  assertEq(f(), 2);
}

test();

var log = '';
try {
  // Redeclaring an explicitly 'let'-declared binding doesn't work.
  evaluate(`{
    let x = 42;
    function x() {}
  }`);
} catch (e) {
  assertEq(e instanceof SyntaxError, true);
  log += 'e';
}

try {
  // Redeclaring an explicitly 'const'-declared binding doesn't work.
  evaluate(`{
    const x = 42;
    function x() {}
  }`);
} catch (e) {
  assertEq(e instanceof SyntaxError, true);
  log += 'e';
}

assertEq(log, 'ee');

if ('reportCompare' in this)
  reportCompare(true, true);
