if (helperThreadCount() == 0)
    quit();

evalInWorker(`
  function f() {
    fullcompartmentchecks(f);
  }
  try { f(); } catch(e) {}
`);
