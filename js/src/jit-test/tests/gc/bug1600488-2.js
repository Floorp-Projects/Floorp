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
assertEq(finalizationRegistry.unregister(token), true);
finalizationRegistry.cleanupSome();
assertEq(iterated, undefined);
