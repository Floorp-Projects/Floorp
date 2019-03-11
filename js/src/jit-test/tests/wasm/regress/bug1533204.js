enableOsiPointRegisterChecks();
evalInWorker(`
function DiagModule(stdlib, foreign) {
    "use asm";
    function diag() {
        while(1) {}
    }
    return {};
`);
