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

Object.defineProperty(Boolean.prototype, "v",
                      { get() { "use strict"; return typeof this; },
                        enumerable: true,
                        configurable: true });

Object.defineProperty(Number.prototype, "v",
                      { get() { "use strict"; return typeof this; },
                        enumerable: true,
                        configurable: true });

Object.defineProperty(String.prototype, "v",
                      { get() { "use strict"; return typeof this; },
                        enumerable: true,
                        configurable: true });

Object.defineProperty(Symbol.prototype, "v",
                      { get() { "use strict"; return typeof this; },
                        enumerable: true,
                        configurable: true });

function primitiveThisSupported()
{
  return 3.14.custom === "number";
}

function primitiveThisTests()
{
  function f(v)
  {
    var type = typeof v;

    ({ v } = v);

    assertEq(v, type);
  }

  f(true);
  f(3.14);
  f(72);
  f("ohai");
  f(Symbol.iterator);

  assertThrowsInstanceOf(() => f(undefined), TypeError);
  assertThrowsInstanceOf(() => f(null), TypeError);

  function g(v)
  {
    var type = typeof v;

    ({ v } = v);

    assertEq(v, type);
  }

  g(true);
  g(3.14);
  g(72);
  g("ohai");
  g(Symbol.iterator);

  assertThrowsInstanceOf(() => g(null), TypeError);
  assertThrowsInstanceOf(() => g(undefined), TypeError);
}
if (primitiveThisSupported())
  primitiveThisTests();

