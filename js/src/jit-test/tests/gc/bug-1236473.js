// |jit-test| skip-if: !('oomTest' in this)

oomTest(() => {
    offThreadCompileScript(`try {} catch (NaN) {}`);
    runOffThreadScript();
});
