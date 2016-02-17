if (helperThreadCount() === 0)
    quit();
evalInWorker("setInterruptCallback(function() {}); timeout(1000);");
