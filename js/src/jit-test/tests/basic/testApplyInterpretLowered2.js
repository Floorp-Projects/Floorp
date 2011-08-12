
/* Make sure the interpreter can pop lowered frames pushed by CompileFunction. */

function recompile() {}

function bar() {
  for (var i = 0; i < 50; i++) {
    var foo = new Function("recompile(arguments[0] + " + i + "); return arguments[0]");
    assertEq(foo.apply(null, [i]), i);
  }
}
bar();
