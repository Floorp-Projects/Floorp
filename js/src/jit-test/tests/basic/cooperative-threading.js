if (helperThreadCount() === 0)
  quit();
evalInCooperativeThread("var x = 3");
evalInCooperativeThread("cooperativeYield()");
cooperativeYield();
evalInCooperativeThread("cooperativeYield(); gc();");
gc();
