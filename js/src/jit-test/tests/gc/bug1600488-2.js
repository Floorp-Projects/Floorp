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
assertEq(finalizationGroup.unregister(token), true);
finalizationGroup.cleanupSome();
assertEq(iterated, undefined);
