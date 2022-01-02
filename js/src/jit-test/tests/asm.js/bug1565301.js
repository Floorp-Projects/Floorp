// |jit-test| skip-if: !isAsmJSCompilationAvailable() || helperThreadCount() === 0

const maxSize = Math.pow(2, 29) - 1;

// We just don't want to crash during compilation here.

evaluate(`
  offThreadCompileScript("\\
    g = (function(t,foreign){\\
        \\"use asm\\";\\
        var ff = foreign.ff;\\
        function f() {\\
            +ff()\\
        }\\
        return f\\
", { lineNumber: (${maxSize})});
`);

evaluate(`
  offThreadCompileScript("\\
    g = (function(t,foreign){\\
        \\"use asm\\";\\
        var ff = foreign.ff;\\
        function f() {\\
            +ff()\\
        }\\
        return f\\
", { lineNumber: (${maxSize + 1})});
`);
