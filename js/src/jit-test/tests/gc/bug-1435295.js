// |jit-test| skip-if: helperThreadCount() === 0 || !('oomTest' in this)

oomTest(new Function(`function execOffThread(source) {
    offThreadCompileModule(source);
    return finishOffThreadModule();
}
b = execOffThread("[1, 2, 3]")
`));
