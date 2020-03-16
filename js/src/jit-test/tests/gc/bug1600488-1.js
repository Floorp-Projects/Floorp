// |jit-test| --enable-weak-refs

const token = {};
let iterated;
const finalizationRegistry = new FinalizationRegistry(items => {
    iterated = items.next().value;
});
{
    let object = {};
    finalizationRegistry.register(object, token, token);
    object = undefined;
}
gc();
finalizationRegistry.cleanupSome();
assertEq(iterated, token);
assertEq(finalizationRegistry.unregister(token), false);
