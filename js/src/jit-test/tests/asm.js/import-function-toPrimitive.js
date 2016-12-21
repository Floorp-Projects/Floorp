var counter = 0;

function f(stdlib, foreign)
{
  "use asm";
  var a = +foreign.a;
  var b = +foreign.b;
  function g() {}
  return g;
}

var foreign =
  {
    a: function() {},
    b: /value that doesn't coerce purely/,
  };

foreign.a[Symbol.toPrimitive] =
  function()
  {
    counter++;
    return 0;
  };

f(null, foreign);
assertEq(counter, 1);
