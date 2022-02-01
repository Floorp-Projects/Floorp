// |jit-test| skip-if: !('oomTest' in this) || helperThreadCount() === 0

try {
    oomTest(function() {
      eval(`
        function eval(source) {
          offThreadCompileModuleToStencil(source);
          minorgc();
        }
        eval("");
      `);
    });
} catch (exc) {}
