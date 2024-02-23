// |jit-test| skip-if: !hasFunction.oomAfterAllocations

asyncFunc1("geval0\n await ''")
async function asyncFunc1(lfVarx) {
  lfMod = parseModule(lfVarx);
  moduleLink(lfMod);
  await moduleEvaluate(lfMod);
}
oomAfterAllocations(1);
