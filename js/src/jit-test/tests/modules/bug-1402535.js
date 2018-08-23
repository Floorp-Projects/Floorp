if (!('stackTest' in this))
   quit();

stackTest(function() {
    let m = parseModule(``);
    instantiateModule(m);
});
