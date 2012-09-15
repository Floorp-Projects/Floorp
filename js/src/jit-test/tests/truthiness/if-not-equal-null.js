var counterF = 0;

function f(v, value)
{
  if (v != null)
    counterF++;
  assertEq(counterF, value,
           "failed: " + v + " " + value);
}

f({}, 1);
f({}, 2);
f(null, 2);
f(null, 2);
f(undefined, 2);
f(undefined, 2);
f(objectEmulatingUndefined(), 2);
f(objectEmulatingUndefined(), 2);
f(Object.prototype, 3);
f(Object.prototype, 4);

var counterG = 0;

function g(v, value)
{
  if (v != null)
    counterG++;
  assertEq(counterG, value,
           "failed: " + v + " " + value);
}

g({}, 1);
g({}, 2);

var counterH = 0;

function h(v, value)
{
  if (v != null)
    counterH++;
  assertEq(counterH, value,
           "failed: " + v + " " + value);
}

h(objectEmulatingUndefined(), 0);
h(objectEmulatingUndefined(), 0);
