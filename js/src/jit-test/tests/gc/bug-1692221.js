// |jit-test| allow-oom; skip-if: !('oomAtAllocation' in this)

// Test TenuredChunk::decommitFreeArenasWithoutUnlocking updates chunk
// metadata correctly. The data is checked by assertions so this test is about
// exercising the code in question.

function allocateGarbage() {
  gc();
  for (let j = 0; j < 100000; j++) {
    Symbol();
  }
}

function collectUntilDecommit() {
  startgc(1);
  while (gcstate() != "NotActive" && gcstate() != "Decommit") {
    gcslice(1000);
  }
}

function triggerSyncDecommit() {
  reportLargeAllocationFailure(1);
}

gczeal(0);

// Normally we skip decommit if GCs are happening frequently. Disable that for
// this test
gcparam("highFrequencyTimeLimit", 0);

allocateGarbage();
collectUntilDecommit();
triggerSyncDecommit();

allocateGarbage();
collectUntilDecommit();
oomAtAllocation(10);
triggerSyncDecommit();
resetOOMFailure();
