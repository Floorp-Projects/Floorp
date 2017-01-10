if (helperThreadCount() === 0)
   quit();
evalInWorker(`schedulegc("s1");`);
