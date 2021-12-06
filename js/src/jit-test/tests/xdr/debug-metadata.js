var g = newGlobal({ newCompartment: true });
var dbg = new Debugger(g);
var count = 0;

dbg.onNewScript = script => {
  var source = script.source;
  assertEq(source.element.getOwnPropertyDescriptor("name").value, "hello");
  assertEq(source.elementAttributeName, "world");
  count++;
};

var stencil = g.compileToStencil(`"a"`);
g.evalStencil(stencil, {
  element: { name: "hello" },
  elementAttributeName: "world",
});
assertEq(count, 1);

count = 0;
dbg.onNewScript = script => {
  var source = script.source;
  assertEq(source.element.getOwnPropertyDescriptor("name").value, "HELLO");
  assertEq(source.elementAttributeName, "WORLD");
  count++;
};

var stencilXDR = g.compileToStencilXDR(`"b"`);
g.evalStencilXDR(stencilXDR, {
  element: { name: "HELLO" },
  elementAttributeName: "WORLD",
});
assertEq(count, 1);
