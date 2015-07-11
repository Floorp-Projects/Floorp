// Test basic usage of drainTenurePromotionsLog() with various objects.

const root = newGlobal();
const dbg = new Debugger();
const wrappedRoot = dbg.addDebuggee(root)

dbg.memory.trackingAllocationSites = true;
dbg.memory.trackingTenurePromotions = true;

root.eval(`
  (function immediate() {
    this.tests = [
      { name: "({})",       cls: "Object", obj: ({})                  },
      { name: "[]",         cls: "Array",  obj: []                    },
      { name: "new Object", cls: "Object", obj: new Object()          },
      { name: "new Array",  cls: "Array",  obj: new Array()           },
      { name: "new Date",   cls: "Date",   obj: new Date()            },
    ];
  }());
  minorgc();
`);

const promotions = dbg.memory.drainTenurePromotionsLog();

// Assert that the promotions happen in chronological order.
let lastWhen = -Infinity;
for (let p of promotions) {
  assertEq(p.timestamp >= lastWhen, true);
  lastWhen = p.timestamp;
}

for (let { name, cls, obj } of root.tests) {
  print(name + ": " + cls);

  // The promoted object's allocation site should be in the log.
  let wrappedObj = wrappedRoot.makeDebuggeeValue(obj);
  let allocSite = wrappedObj.allocationSite;
  let idx = promotions.map(x => x.frame).indexOf(allocSite);
  assertEq(idx >= 0, true);

  // The promoted object's class name should be present.
  assertEq(promotions.map(x => x.class).indexOf(cls) >= 0, true);
}
