// Make sure arguments object handlig deals well with aliasing of args.
var arr = [];
function f(x) {
  var args = arguments;
  arr.push(arguments);
  arguments[0] = 0;
  return {
    f: function () { return x; },
    g: function () { return args[0]; }
  };
}

// Check that aliased arguments are correctly set to the callObject
for (var i = 0; i < 2000; i++)
  assertEq(f(1).f(), 0);

// Check that aliased arguments are correctly read from the callObject
for (var i = 0; i < 2000; i++)
  assertEq(f(1).g(), 0);
