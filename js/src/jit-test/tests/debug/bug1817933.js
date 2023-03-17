var g = newGlobal({"newCompartment": true});
const dbg = new g.Debugger(this);

with ({x: 3}) {
  function foo(n) {
    () => {n}

    if (n < 20) {
      foo(n+1);
    } else {
      dbg.getNewestFrame().eval("var x = 23;");
    }
    +x
  }
  foo(0);
}
