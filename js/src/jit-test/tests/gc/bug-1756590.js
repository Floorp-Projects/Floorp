// |jit-test| error: Error

function asyncGC(...targets) {
  var finalizationRegistry = new FinalizationRegistry(() => {});
  for (let target of targets) {
    finalizationRegistry.register(target, 'target');
  }
  return Promise.resolve('tick').then(() => asyncGCDeref()).then(() => {
    finalizationRegistry.cleanupSome(name => { names.push(name); });
  });
}
const root = newGlobal({newCompartment: true});
const dbg = new Debugger();
dbg.each = asyncGC;
const wrappedRoot = dbg.each (root)
gczeal(14,10);
evaluate(`
  var StructType = class {};
`);
