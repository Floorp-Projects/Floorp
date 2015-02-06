// Test that SavedFrame.prototype.parent gives the next older frame whose
// principals are subsumed by the caller's principals.

// Given a string of letters |expected|, say "abc", assert that the stack
// contains calls to a series of functions named by the next letter from
// the string, say a, b, and then c. Younger frames appear earlier in
// |expected| than older frames.
function check(expected, stack) {
  print("check(" + uneval(expected) + ") against:\n" + stack);
  count++;

  while (stack.length && expected.length) {
    assertEq(stack.shift(), expected[0]);
    expected = expected.slice(1);
  }

  if (expected.length > 0) {
    throw new Error("Missing frames for: " + expected);
  }
  if (stack.length > 0 && !stack.every(s => s === null)) {
    throw new Error("Unexpected extra frame(s):\n" + stack);
  }
}

// Go from a SavedFrame linked list to an array of function display names.
function extract(stack) {
  const results = [];
  while (stack) {
    results.push(stack.functionDisplayName);
    stack = stack.parent;
  }
  return results;
}

const low  = newGlobal({ principal: 0       });
const mid  = newGlobal({ principal: 0xffff  });
const high = newGlobal({ principal: 0xfffff });

var count = 0;

     eval('function a() { check("a",        extract(saveStack())); b(); }');
low .eval('function b() { check("b",        extract(saveStack())); c(); }');
mid .eval('function c() { check("cba",      extract(saveStack())); d(); }');
high.eval('function d() { check("dcba",     extract(saveStack())); e(); }');
     eval('function e() { check("edcba",    extract(saveStack())); f(); }'); // no principal, so checks skipped
low .eval('function f() { check("fb",       extract(saveStack())); g(); }');
mid .eval('function g() { check("gfecba",   extract(saveStack())); h(); }');
high.eval('function h() { check("hgfedcba", extract(saveStack()));      }');

// Make everyone's functions visible to each other, as needed.
     b = low .b;
low .c = mid .c;
mid .d = high.d;
high.e =      e;
     f = low .f;
low .g = mid .g;
mid .h = high.h;

low.check = mid.check = high.check = check;

// They each must have their own extract so that CCWs don't mess up the
// principals when we ask for the parent property.
low. eval("" + extract);
mid. eval("" + extract);
high.eval("" + extract);

// Kick the whole process off.
a();

assertEq(count, 8);
