if (!('oomTest' in this))
  quit();

oomTest(() => {
    offThreadCompileScript(`try {} catch (NaN) {}`);
    runOffThreadScript();
});
