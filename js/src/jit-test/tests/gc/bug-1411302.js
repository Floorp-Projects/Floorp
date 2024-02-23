let lfPreamble = `
    value:{
`;
try {
    evaluate("");
    evalInWorker("");
} catch (exc) {}
try {
    evalInWorker("");
} catch (exc) {}
try {
    oomTest(function() {
        eval("function testDeepBail1() {");
    });
} catch (exc) {}
