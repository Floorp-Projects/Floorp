// Test usage of drainTenurePromotionsLog() with multiple debuggers.

const root = newGlobal();
const dbg1 = new Debugger(root);
const dbg2 = new Debugger(root);

function setTracking(dbg, bool) {
  dbg.memory.trackingAllocationSites = bool;
  dbg.memory.trackingTenurePromotions = bool;
}

function tenureObjectsAndDrainLog() {
  gc();

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

  const promotions2 = dbg2.memory.trackingTenurePromotions ? dbg2.memory.drainTenurePromotionsLog() : [];
  const promotions1 = dbg1.memory.trackingTenurePromotions ? dbg1.memory.drainTenurePromotionsLog() : [];

  setTracking(dbg1, false);
  setTracking(dbg2, false);

  return { promotions1, promotions2 };
}

{
  setTracking(dbg1, true);
  setTracking(dbg2, true);
  const { promotions1, promotions2 } = tenureObjectsAndDrainLog();

  // Should get logs in dbg1,
  assertEq(!!promotions1.length, true);
  // and logs in dbg2.
  assertEq(!!promotions2.length, true);
}

{
  setTracking(dbg1, false);
  setTracking(dbg2, false);
  const { promotions1, promotions2 } = tenureObjectsAndDrainLog();

  // Should *not* get logs in dbg1,
  assertEq(!!promotions1.length, false);
  // nor logs in dbg2.
  assertEq(!!promotions2.length, false);
}

{
  setTracking(dbg1, true);
  setTracking(dbg2, false);
  const { promotions1, promotions2 } = tenureObjectsAndDrainLog();

  // Should get logs in dbg1,
  assertEq(!!promotions1.length, true);
  // but *not* logs in dbg2.
  assertEq(!!promotions2.length, false);
}

{
  setTracking(dbg1, false);
  setTracking(dbg2, true);
  const { promotions1, promotions2 } = tenureObjectsAndDrainLog();

  // Should *not* get logs in dbg1,
  assertEq(!!promotions1.length, false);
  // but *should* get logs in dbg2.
  assertEq(!!promotions2.length, true);
}
