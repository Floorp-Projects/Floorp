// |jit-test| skip-if: !('oomAfterAllocations' in this)

asyncFunc1("geval0\n await ''")
async function asyncFunc1(lfVarx) {
  lfMod = parseModule(lfVarx);
  lfMod.declarationInstantiation();
  await lfMod.evaluation();
}
oomAfterAllocations(1);