// Test draining tenure log without tracking tenures.

const root = newGlobal();
const dbg = new Debugger(root);

let threw = false;
try {
  dbg.drainTenurePromotionsLog();
} catch (e) {
  threw = true;
}
assertEq(threw, true);
