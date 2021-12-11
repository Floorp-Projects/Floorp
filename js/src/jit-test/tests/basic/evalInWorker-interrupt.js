// |jit-test| skip-if: helperThreadCount() === 0
evalInWorker("setInterruptCallback(function() {}); timeout(1000);");
