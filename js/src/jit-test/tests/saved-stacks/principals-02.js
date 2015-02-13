// Test that SavedFrame.prototype.toString only shows frames whose principal is
// subsumed by the caller's principal.

var count = 0;

// Given a string of letters |expected|, say "abc", assert that the stack
// contains calls to a series of functions named by the next letter from
// the string, say a, b, and then c. Younger frames appear earlier in
// |expected| than older frames.
function check(expected, stack) {
  print("check(" + uneval(expected) + ") against:\n" + stack);
  count++;

  // Extract only the function names from the stack trace. Omit the frames
  // for the top-level evaluation, if it is present.
  const frames = stack
    .split("\n")
    .filter(f => f.match(/^.@/))
    .map(f => f.replace(/@.*$/g, ""));

  // Check the function names against the expected sequence.
  assertEq(frames.length, expected.length);
  for (var i = 0; i < expected.length; i++) {
    assertEq(frames[i], expected[i]);
  }
}

var low = newGlobal({ principal: 0 });
var mid = newGlobal({ principal: 0xffff });
var high = newGlobal({ principal: 0xfffff });

     eval('function a() { check("a",        saveStack().toString()); b(); }');
low .eval('function b() { check("b",        saveStack().toString()); c(); }');
mid .eval('function c() { check("cba",      saveStack().toString()); d(); }');
high.eval('function d() { check("dcba",     saveStack().toString()); e(); }');
     eval('function e() { check("ecba",     saveStack().toString()); f(); }');
low .eval('function f() { check("fb",       saveStack().toString()); g(); }');
mid .eval('function g() { check("gfecba",   saveStack().toString()); h(); }');
high.eval('function h() { check("hgfedcba", saveStack().toString());      }');

// Make everyone's functions visible to each other, as needed.
     b = low .b;
low .c = mid .c;
mid .d = high.d;
high.e =      e;
     f = low .f;
low .g = mid .g;
mid .h = high.h;

low.check = mid.check = high.check = check;

// Kick the whole process off.
a();

assertEq(count, 8);

