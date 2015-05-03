function run_test()
{
  try {
    var sandbox = new Components.utils.Sandbox(null, {"sandboxPrototype" : {}});
    do_check_true(false);
  } catch (e) {
    do_check_true(/must subsume sandboxPrototype/.test(e));
  }
}
