if (!('stackTest' in this))
   quit();

let a = parseModule(`throw new Error`);
instantiateModule(a);
stackTest(function() {
    evaluateModule(a);
});
