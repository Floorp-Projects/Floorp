if (helperThreadCount() === 0)
    quit();
if (!('oomTest' in this))
    quit();

oomTest(new Function(`function execOffThread(source) {
    offThreadCompileModule(source);
    return finishOffThreadModule();
}
b = execOffThread("[1, 2, 3]")
`));
