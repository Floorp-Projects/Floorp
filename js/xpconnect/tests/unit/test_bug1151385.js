function run_test()
{
  try {
    var sandbox = new Components.utils.Sandbox(null, {"sandboxPrototype" : {}});
    Assert.ok(false);
  } catch (e) {
    Assert.ok(/must subsume sandboxPrototype/.test(e));
  }
}
