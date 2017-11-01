if (helperThreadCount() === 0)
  quit();
evalInCooperativeThread(`
      const dbg = new Debugger();
      evalInWorker("");
`);
