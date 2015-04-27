// Test drainAllocationsLog() and constructor names.

const root = newGlobal();
const dbg = new Debugger();
const wrappedRoot = dbg.addDebuggee(root);

root.eval(
  `
  function Ctor() {}

  var nested = {};
  nested.Ctor = function () {};

  function makeInstance() {
    let LexicalCtor = function () {};
    return new LexicalCtor;
  }

  function makeObject() {
    let object = {};
    return object;
  }

  this.tests = [
    { name: "Ctor",                     fn: () => new Ctor             },
    { name: "nested.Ctor",              fn: () => new nested.Ctor      },
    { name: "makeInstance/LexicalCtor", fn: () => makeInstance()       },
    { name: null,                       fn: () => ({})                 },
    { name: null,                       fn: () => (nested.object = {}) },
    { name: null,                       fn: () => makeObject()         },
  ];
  `
);

for (let { name, fn } of root.tests) {
  print(name);

  dbg.memory.trackingAllocationSites = true;

  fn();

  let entries = dbg.memory.drainAllocationsLog();
  let ctors = entries.map(e => e.constructor);
  assertEq(ctors.some(ctor => ctor === name), true);

  dbg.memory.trackingAllocationSites = false;
}
