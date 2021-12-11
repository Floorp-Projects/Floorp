function f(v, value)
{
  var b = v === null;
  assertEq(b, value,
           "failed: " + v + " " + value);
}

f({}, false);
f({}, false);
f(null, true);
f(null, true);
f(undefined, false);
f(undefined, false);
f(createIsHTMLDDA(), false);
f(createIsHTMLDDA(), false);
f(Object.prototype, false);
f(Object.prototype, false);

function g(v, value)
{
  var b = v === null;
  assertEq(b, value,
           "failed: " + v + " " + value);
}

g({}, false);
g({}, false);

function h(v, value)
{
  var b = v === null;
  assertEq(b, value,
           "failed: " + v + " " + value);
}

h(createIsHTMLDDA(), false);
h(createIsHTMLDDA(), false);
