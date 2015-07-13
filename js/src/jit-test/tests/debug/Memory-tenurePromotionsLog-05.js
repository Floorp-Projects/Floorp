// Test that objects that do not get tenured do not go in the log.

// Can't let gc zeal mess with our assumptions about gc behavior here.
gczeal(0);

const root = newGlobal();
const dbg = root.dbg = new Debugger(root);

root.eval(
  `
  this.forceLazyInstantiation = new Date;
  gc();

  this.allocTemp = function allocTemp() {
    gc();
    this.dbg.memory.trackingTenurePromotions = true;
    // Create an object, remove references to it, and then do a minor gc. It
    // should not be in the tenure promotions log because it should have died in
    // the nursery. We use a Date object so it is easier to find when asserting.
    var d = new Date;
    d = null;
    minorgc();
  };
  `
);

root.allocTemp();

const promotions = dbg.memory.drainTenurePromotionsLog();
assertEq(promotions.some(e => e.class === "Date"), false);
