if (!('oomTest' in this))
  quit();

enableSPSProfiling();
oomTest(function() {
    eval("(function() {})()")
});
