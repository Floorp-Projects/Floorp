// |jit-test| --enable-weak-refs

gczeal(4);
let heldValues = [];
group = new FinalizationGroup(iterator => {
    heldValues.push(...iterator);
});
group.register({}, 42);
gc();
