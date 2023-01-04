gczeal(4);
evaluate(` 
  for (x = 1; x < 2; ++x)
    gcparam("maxHelperThreads", 1)
`);
