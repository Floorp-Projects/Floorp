// |jit-test| skip-if: helperThreadCount() === 0
offThreadCompileScript(``);
evalInWorker(`runOffThreadScript()`);
