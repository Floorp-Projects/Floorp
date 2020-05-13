// |jit-test| --enable-weak-refs

gczeal(4);
let heldValues = [];
registry = new FinalizationRegistry(iterator => {
    heldValues.push(...iterator);
});
registry.register({}, 42);
gc();
