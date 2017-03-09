if (helperThreadCount() === 0)
  quit();
evalInCooperativeThread('cooperativeYield(); var dbg = new Debugger();');
