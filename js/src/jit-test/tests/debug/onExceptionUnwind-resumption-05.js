// Make sure that stacks in completion values are not lost when an exception
// unwind hook returns undefined.

let g = newGlobal({ newCompartment: true });
g.eval(`
  function foo() {
    bar();
  }
  function bar() {
    throw new Error();
  }
`);

let dbg = Debugger(g);
let unwindHits = 0, popHits = 0;
dbg.onExceptionUnwind = frame => {
  unwindHits++;
  return undefined;
}
dbg.onEnterFrame = frame => {
  frame.onPop = completion => {
    assertEq(completion.stack.functionDisplayName, "bar");
    popHits++;
  };
};

try {
  g.eval("foo()");
} catch (e) {}
assertEq(unwindHits, 3);
assertEq(popHits, 3);
dbg.removeDebuggee(g);
