try{} catch (x) {}

var callStack = [];
function currentFunc() {
    return callStack[0];
}
function reportFailure () {
    var funcName = currentFunc();
    // play with the result to cause a SEGV.
    var prefix = (funcName) ? funcName : "";
    // Use OSR to compile the function at the end of the first run.
    for (var i=0; i < 50; i++) ;
}

callStack[0] = 'test';
// Run and compile with a string as result of currentFunc.
reportFailure();
callStack[0] = undefined;
// Use previously compiled code with the string assumption.
reportFailure();
