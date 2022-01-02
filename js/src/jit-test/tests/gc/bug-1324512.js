// |jit-test| skip-if: helperThreadCount() === 0

evalInWorker(`
    if (!('gczeal' in this))
        quit();
    try {
      gczeal(2,1);
      throw new Error();
    } catch (e) {
      assertEq("" + e, "Error");
    }
`);
