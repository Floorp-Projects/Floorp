// |jit-test| skip-if: helperThreadCount() === 0

evalInWorker(`
  function f() {
    fullcompartmentchecks(f);
  }
  try { f(); } catch(e) {}
`);
