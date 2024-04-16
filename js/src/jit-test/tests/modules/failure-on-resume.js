const dbgGlobal = newGlobal({ newCompartment: true });
dbgGlobal.parent = this;
dbgGlobal.eval(`
var entered = 0;
var forceReturn = false;
Debugger(parent).onEnterFrame = function () {
  entered++;
  if (forceReturn) {
    return { return: "force return" };
  }
  return undefined;
};
`);

{
  function* f1() { yield 10; };

  dbgGlobal.entered = 0;
  let g = f1();
  assertEq(dbgGlobal.entered, 1);
  dbgGlobal.forceReturn = true;
  let ret = g.next();
  dbgGlobal.forceReturn = false;
  assertEq(dbgGlobal.entered, 2);

  assertEq(ret.value, "force return");
}

{
  async function f2() { await {}; }

  dbgGlobal.entered = 0;
  let p = f2();
  assertEq(dbgGlobal.entered, 1);
  dbgGlobal.forceReturn = true;
  drainJobQueue();
  dbgGlobal.forceReturn = false;
  assertEq(dbgGlobal.entered, 2);

  let ret = null;
  p.then(x => ret = x);
  drainJobQueue();
  assertEq(ret, "force return");
}

{
  async function* f3() { await {}; }

  dbgGlobal.entered = 0;
  let g = f3();
  assertEq(dbgGlobal.entered, 1);
  dbgGlobal.forceReturn = true;
  let p = g.next();
  dbgGlobal.forceReturn = false;
  assertEq(dbgGlobal.entered, 2);

  let ret = null;
  p.then(v => ret = v);
  drainJobQueue();
  assertEq(ret.value, "force return");
}

{
  let m = registerModule("1", parseModule("await {};"));
  moduleLink(m);

  dbgGlobal.entered = 0;
  let p = moduleEvaluate(m);
  assertEq(dbgGlobal.entered, 1);
  dbgGlobal.forceReturn = true;
  drainJobQueue();
  dbgGlobal.forceReturn = false;
  assertEq(dbgGlobal.entered, 2);

  let ret = null;
  p.then(x => ret = x);
  drainJobQueue();
  assertEq(ret, undefined);
}
