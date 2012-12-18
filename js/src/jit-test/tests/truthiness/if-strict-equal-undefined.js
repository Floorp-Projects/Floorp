var counterF = 0;

function f(v, value)
{
  if (v === undefined)
    counterF++;
  assertEq(counterF, value,
           "failed: " + v + " " + value);
}

f({}, 0);
f({}, 0);
f(null, 0);
f(null, 0);
f(undefined, 1);
f(undefined, 2);
f(objectEmulatingUndefined(), 2);
f(objectEmulatingUndefined(), 2);
f(Object.prototype, 2);
f(Object.prototype, 2);

var counterG = 0;

function g(v, value)
{
  if (v === undefined)
    counterG++;
  assertEq(counterG, value,
           "failed: " + v + " " + value);
}

g({}, 0);
g({}, 0);

var counterH = 0;

function h(v, value)
{
  if (v === undefined)
    counterH++;
  assertEq(counterH, value,
           "failed: " + v + " " + value);
}

h(objectEmulatingUndefined(), 0);
h(objectEmulatingUndefined(), 0);
