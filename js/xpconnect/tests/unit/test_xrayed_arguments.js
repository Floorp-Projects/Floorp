function run_test() {
  var sbContent = Cu.Sandbox(null);
  let xrayedArgs = sbContent.eval("(function(a, b) { return arguments; })('hi', 42)");

  function checkArgs(a) {
    Assert.equal(a.length, 2);
    Assert.equal(a[0], 'hi');
    Assert.equal(a[1], 42);
  }

  // Check Xrays to the args.
  checkArgs(xrayedArgs);

  // Make sure the spread operator works.
  checkArgs([...xrayedArgs]);
}
