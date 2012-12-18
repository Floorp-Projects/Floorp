var counterF = 0;

function f(v, value)
{
  if (v !== null)
    counterF++;
  assertEq(counterF, value,
           "failed: " + v + " " + value);
}

f({}, 1);
f({}, 2);
f(null, 2);
f(null, 2);
f(undefined, 3);
f(undefined, 4);
f(objectEmulatingUndefined(), 5);
f(objectEmulatingUndefined(), 6);
f(Object.prototype, 7);
f(Object.prototype, 8);

var counterG = 0;

function g(v, value)
{
  if (v !== null)
    counterG++;
  assertEq(counterG, value,
           "failed: " + v + " " + value);
}

g({}, 1);
g({}, 2);

var counterH = 0;

function h(v, value)
{
  if (v !== null)
    counterH++;
  assertEq(counterH, value,
           "failed: " + v + " " + value);
}

h(objectEmulatingUndefined(), 1);
h(objectEmulatingUndefined(), 2);
