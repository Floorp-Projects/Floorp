new FinalizationRegistry(() => 0).register(newGlobal({newCompartment: true}));
recomputeWrappers();
