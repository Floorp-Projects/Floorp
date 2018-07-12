if (!('oomTest' in this) || helperThreadCount() === 0)
    quit();

try {
    oomTest(function() {
      eval(`
        function eval(source) {
          offThreadCompileModule(source);
          minorgc();
        }
        eval("");
      `);
    });
} catch (exc) {}
