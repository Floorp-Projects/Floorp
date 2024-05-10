// |jit-test| skip-if: helperThreadCount() === 0
evalInWorker(`
  enqueueMark("drain")
  startgc()
`)
