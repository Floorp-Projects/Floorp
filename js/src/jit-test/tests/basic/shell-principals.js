// Test the JS shell's toy principals.

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
  var split = stack.split(/(.)?@.*\n/).slice(0, -1);
  if (split[split.length - 1] === undefined)
    split = split.slice(0, -2);

  // Check the function names against the expected sequence.
  assertEq(split.length, expected.length * 2);
  for (var i = 0; i < expected.length; i++)
    assertEq(split[i * 2 + 1], expected[i]);
}

var low = newGlobal({ principal: 0 });
var mid = newGlobal({ principal: 0xffff });
var high = newGlobal({ principal: 0xfffff });

     eval('function a() { check("a",        Error().stack); b(); }');
low .eval('function b() { check("b",        Error().stack); c(); }');
mid .eval('function c() { check("cba",      Error().stack); d(); }');
high.eval('function d() { check("dcba",     Error().stack); e(); }');

// Globals created with no explicit principals get 0xffff.
     eval('function e() { check("ecba",     Error().stack); f(); }');

low .eval('function f() { check("fb",       Error().stack); g(); }');
mid .eval('function g() { check("gfecba",   Error().stack); h(); }');
high.eval('function h() { check("hgfedcba", Error().stack);      }');

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
