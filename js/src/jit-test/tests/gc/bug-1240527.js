if (typeof offThreadCompileScript !== 'function' ||
    typeof runOffThreadScript !== 'function' ||
    typeof oomTest !== 'function' ||
    typeof fullcompartmentchecks !== 'function' ||
    helperThreadCount() === 0)
{
    quit(0);
}

offThreadCompileScript(`
 oomTest(() => "".search(/d/));
 fullcompartmentchecks(3);
`);
runOffThreadScript();
