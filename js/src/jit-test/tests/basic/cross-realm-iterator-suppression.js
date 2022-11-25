function foo(o) {
  // Ensure a NativeIterator exists for o.
  for (var s in o) {}

  // Loop over the keys, deleting y.
  // If y is correctly suppressed, then result should
  // be the name of the first property.
  var result = "";
  for (var s in o) {
    delete o.y;
    result += s;
  }
  assertEq(o[result], 42);
}

with ({}) {}
for (var i = 0; i < 100; i++) {
  var o = {};
  o["x" + i] = 42;
  o.y = 1;
  foo(o);
}

// Cross-realm, same-compartment
var g = newGlobal({newCompartment: false});
g.eval("var o = {x: 42, y: 1}");
foo(g.o);

// Cross-realm, cross-compartment
var g2 = newGlobal({newCompartment: true});
g2.eval("var o = {x: 42, y: 1}");
foo(g2.o);
