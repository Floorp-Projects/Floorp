// |jit-test| --enable-weak-refs

const token = {};
let iterated;
const finalizationGroup = new FinalizationGroup(items => {
    iterated = items.next().value;
});
{
    let object = {};
    finalizationGroup.register(object, token, token);
    object = undefined;
}
gc();
finalizationGroup.cleanupSome();
assertEq(iterated, token);
assertEq(finalizationGroup.unregister(token), false);
