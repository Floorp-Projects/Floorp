fullcompartmentchecks(true);
let a = new FinalizationRegistry(b => {});
let c = newGlobal({newCompartment: true});
a.register(c);
