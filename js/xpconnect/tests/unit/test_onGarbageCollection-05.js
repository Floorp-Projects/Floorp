// Test that the onGarbageCollection hook reports its gc cycle's number (aka the
// major GC number) and that it is monotonically increasing.

const root = newGlobal();
const dbg = new Debugger();
const wrappedRoot = dbg.addDebuggee(root)

function run_test() {
  do_test_pending();

  let numFired = 0;
  let lastGCCycleNumber = undefined;

  (function loop() {
    if (numFired == 10) {
      dbg.memory.onGarbageCollection = undefined;
      dbg.enabled = false;
      return void do_test_finished();
    }

    dbg.memory.onGarbageCollection = data => {
      print("onGarbageCollection: " + uneval(data));

      if (numFired != 0) {
        equal(typeof lastGCCycleNumber, "number");
        equal(data.gcCycleNumber - lastGCCycleNumber, 1);
      }

      numFired++;
      lastGCCycleNumber = data.gcCycleNumber;

      executeSoon(loop);
    };

    root.eval("gc(this)");
  }());
}
