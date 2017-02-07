if (!('oomTest' in this))
    quit();
oomTest(function () {
    offThreadCompileModule('');
    finishOffThreadModule();
});
