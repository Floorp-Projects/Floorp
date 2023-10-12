gczeal(0);

function f() {
  let global = newGlobal({newCompartment: true});
  global.a = Debugger(newGlobal({newCompartment: true}));
  global.evaluate("grayRoot()");
}

function g(i) {
  const str = " ".padStart(10000, " ");
  str.startsWith("1");
  if (i > 0) {
    g(i - 1);
  }
}

f();
gczeal(11,3);
g(1000);
