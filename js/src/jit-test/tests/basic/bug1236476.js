// |jit-test| allow-oom; allow-unhandlable-oom
// 1236476

if (typeof oomTest !== 'function' ||
    typeof offThreadCompileScript !== 'function' ||
    typeof runOffThreadScript !== 'function')
    quit();

oomTest(() => {
    offThreadCompileScript(`
      "use asm";
      return assertEq;
    `);
    runOffThreadScript();
});

