const token = {};
let cleanedUpValue;
const finalizationRegistry = new FinalizationRegistry(value => {
  cleanedUpValue = value;
});
{
    let object = {};
    finalizationRegistry.register(object, token, token);
    object = undefined;
}
gc();
finalizationRegistry.cleanupSome();
assertEq(cleanedUpValue, token);
assertEq(finalizationRegistry.unregister(token), false);
