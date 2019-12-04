// |jit-test| skip-if: helperThreadCount() === 0
evalInWorker(`
  Array.prototype[1] = 1;
  var source = Array(50000).join("(") + "a" + Array(50000).join(")");
  var r70 = RegExp(source);
`);
