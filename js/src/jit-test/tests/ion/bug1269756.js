if (!('oomTest' in this))
    quit();

oomTest(function() {
    m = parseModule(`while (x && NaN) prototype; let x`);
    m.declarationInstantiation();
    m.evaluation();
})
