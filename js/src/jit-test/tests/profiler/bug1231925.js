if (!('oomTest' in this))
  quit();

enableGeckoProfiling();
oomTest(function() {
    eval("(function() {})()")
});
