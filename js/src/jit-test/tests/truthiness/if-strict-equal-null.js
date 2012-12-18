var counterF = 0;

function f(v, value)
{
  if (v === null)
    counterF++;
  assertEq(counterF, value,
           "failed: " + v + " " + value);
}

f({}, 0);
f({}, 0);
f(null, 1);
f(null, 2);
f(undefined, 2);
f(undefined, 2);
f(objectEmulatingUndefined(), 2);
f(objectEmulatingUndefined(), 2);
f(Object.prototype, 2);
f(Object.prototype, 2);

var counterG = 0;

function g(v, value)
{
  if (v === null)
    counterG++;
  assertEq(counterG, value,
           "failed: " + v + " " + value);
}

g({}, 0);
g({}, 0);

var counterH = 0;

function h(v, value)
{
  if (v === null)
    counterH++;
  assertEq(counterH, value,
           "failed: " + v + " " + value);
}

h(objectEmulatingUndefined(), 0);
h(objectEmulatingUndefined(), 0);
