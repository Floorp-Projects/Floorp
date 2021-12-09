// |jit-test| skip-if: helperThreadCount() === 0 || !('oomTest' in this)

offThreadCompileScript(`
 oomTest(() => "".search(/d/));
 fullcompartmentchecks(3);
`);
runOffThreadScript();
