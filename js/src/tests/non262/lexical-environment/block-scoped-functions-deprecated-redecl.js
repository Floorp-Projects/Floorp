{
  assertEq(f(), 4);
  function f() { return 3; }
  assertEq(f(), 4);
  function f() { return 4; }
  assertEq(f(), 4);
}

// Annex B still works.
assertEq(f(), 4);

// The same thing with labels.
{
  assertEq(f(), 4);
  function f() { return 3; }
  assertEq(f(), 4);
  l: function f() { return 4; }
  assertEq(f(), 4);
}

// Annex B still works.
assertEq(f(), 4);

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
  // Strict mode still cannot redeclare.
  eval(`"use strict";
  {
    function f() { }
    function f() { }
  }`);
} catch (e) {
  assertEq(e instanceof SyntaxError, true);
  log += 'e';
}

try {
  // Redeclaring an explicitly 'let'-declared binding doesn't work.
  eval(`{
    let x = 42;
    function x() {}
  }`);
} catch (e) {
  assertEq(e instanceof SyntaxError, true);
  log += 'e';
}

try {
  // Redeclaring an explicitly 'const'-declared binding doesn't work.
  eval(`{
    const x = 42;
    function x() {}
  }`);
} catch (e) {
  assertEq(e instanceof SyntaxError, true);
  log += 'e';
}

assertEq(log, 'eee');

if ('reportCompare' in this)
  reportCompare(true, true);
