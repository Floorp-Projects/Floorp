let fr = new FinalizationRegistry(x => 1);
fr.register(evalcx('({})', newGlobal({newCompartment: true})));
nukeAllCCWs();
gc();
