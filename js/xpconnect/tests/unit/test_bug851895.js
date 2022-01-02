function run_test() {
  // Make sure Components.utils gets its |this| fixed up.
  var isXrayWrapper = Cu.isXrayWrapper;
  Assert.ok(!isXrayWrapper({}), "Didn't throw");

  // Even for classes without |this| fixup, make sure that we don't crash.
  var isSuccessCode = Components.isSuccessCode;
  try { isSuccessCode(Cr.NS_OK); } catch (e) {};
}
