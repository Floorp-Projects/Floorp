// |jit-test| skip-if: !('interruptRegexp' in this) || getBuildConfiguration('pbl')
// skip testing under PBL because interruption of regex engine here seems to
// depend on GC behavior and is hard to reproduce reliably.
var s0 = "A".repeat(10*1024);
var interrupted = false;
gczeal(0);
setInterruptCallback(() => {
  interrupted = true;
  startgc(7,'shrinking');
  return true;
});
assertEq(interruptRegexp(/a(bc|bd)/, s0), null);
assertEq(interrupted, true);
