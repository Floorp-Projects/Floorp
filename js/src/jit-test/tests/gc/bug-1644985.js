let wr = new WeakRef(evalcx('({})', newGlobal({newCompartment: true})));
nukeAllCCWs();
clearKeptObjects();
gc();
