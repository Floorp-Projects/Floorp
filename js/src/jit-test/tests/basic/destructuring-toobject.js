load(libdir + 'asserts.js');
load(libdir + 'iteration.js');

function f(v)
{
  if (v + "")
    ({} = v);
}

f(true);
f({});
assertThrowsInstanceOf(() => f(null), TypeError);
assertThrowsInstanceOf(() => f(undefined), TypeError);

function g(v)
{
  if (v + "")
    ({} = v);
}

g(true);
g({});
assertThrowsInstanceOf(() => g(undefined), TypeError);
assertThrowsInstanceOf(() => g(null), TypeError);

function h(v)
{
  if (v + "")
    ([] = v);
}

h([true]);
h("foo");
assertThrowsInstanceOf(() => h(undefined), TypeError);
assertThrowsInstanceOf(() => h(null), TypeError);

