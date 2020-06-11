gczeal(4);
let heldValues = [];
registry = new FinalizationRegistry(value => {
    heldValues.push(value);
});
registry.register({}, 42);
gc();
