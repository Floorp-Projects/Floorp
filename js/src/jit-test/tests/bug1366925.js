// JS shell shutdown ordering

// Avoid crashing with --no-threads
if (helperThreadCount() == 0)
  quit();

evalInWorker(`
  var lfGlobal = newGlobal();
  lfGlobal.offThreadCompileScript(\`{ let x; throw 42; }\`);
`);
