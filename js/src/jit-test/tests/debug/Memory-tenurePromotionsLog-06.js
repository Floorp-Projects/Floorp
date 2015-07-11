// Test usage of drainTenurePromotionsLog() with multiple debuggees in the same
// zone.

const root1 = newGlobal();
const root2 = newGlobal({ sameZoneAs: root1 });
const dbg = new Debugger();

function tenureObjectsAndDrainLog(root) {
  gc();

  dbg.memory.trackingAllocationSites = true;
  dbg.memory.trackingTenurePromotions = true;

  root.eval(
    `
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
    `
  );

  const promotions = dbg.memory.drainTenurePromotionsLog();

  dbg.memory.trackingAllocationSites = false;
  dbg.memory.trackingTenurePromotions = false;

  return promotions;
}

{
  dbg.addDebuggee(root1);
  dbg.addDebuggee(root2);

  // Should get logs from root1,
  assertEq(!!tenureObjectsAndDrainLog(root1).length, true);
  // and logs from root2.
  assertEq(!!tenureObjectsAndDrainLog(root2).length, true);
}

{
  dbg.removeAllDebuggees();

  // Should *not* get logs from root1,
  assertEq(!!tenureObjectsAndDrainLog(root1).length, false);
  // nor logs from root2.
  assertEq(!!tenureObjectsAndDrainLog(root2).length, false);
}

{
  dbg.addDebuggee(root1);

  // Should get logs from root1,
  assertEq(!!tenureObjectsAndDrainLog(root1).length, true);
  // but *not* logs from root2.
  assertEq(!!tenureObjectsAndDrainLog(root2).length, false);
}

{
  dbg.removeAllDebuggees();
  dbg.addDebuggee(root2);

  // Should *not* get logs from root1,
  assertEq(!!tenureObjectsAndDrainLog(root1).length, false);
  // but should get logs from root2.
  assertEq(!!tenureObjectsAndDrainLog(root2).length, true);
}
