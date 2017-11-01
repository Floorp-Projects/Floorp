if (helperThreadCount() === 0)
  quit();
evalInCooperativeThread(`
startgc(9469, "shrinking");
offThreadCompileScript("");
`);
