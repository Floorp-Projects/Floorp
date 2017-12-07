if (!('stackTest' in this))
   quit();

let a = parseModule(`throw new Error`);
a.declarationInstantiation();
stackTest(function() {
    a.evaluation();
});
