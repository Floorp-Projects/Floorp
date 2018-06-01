if (helperThreadCount() === 0)
    quit();
for (let x = 0; x < 99; ++x)
    evalInWorker("newGlobal().offThreadCompileScript(\"{}\");");
