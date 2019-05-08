// Test exception stack behavior when reusing completion values as resumption
// values.

load(libdir + "asserts.js");

var g = newGlobal({newCompartment: true});
g.eval(`
  function foo() {
    bar();
  }
  function bar() {
    debugger;
  }
  function baz() {
    throw new Error();
  }
`);

var dbg = Debugger(g);
dbg.onDebuggerStatement = frame => {
  return frame.eval("baz()");
};

let popHits = 0;
dbg.onEnterFrame = frame => {
  frame.onPop = completion => {
    popHits++;
    // Resumption values ignore any 'stack' property, and the script location of
    // the place where the hook was called will be used when throwing.
    if (popHits <= 2) {
      assertEq(completion.stack.functionDisplayName, "baz");
    } else {
      assertEq(completion.stack.functionDisplayName, "bar");
    }
  };
};

try {
  g.eval("foo()");
} catch (e) {}
assertEq(popHits, 5);
