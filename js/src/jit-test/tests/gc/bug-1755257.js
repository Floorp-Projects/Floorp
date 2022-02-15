new FinalizationRegistry(a => 1).register(newGlobal({newCompartment: true}))
