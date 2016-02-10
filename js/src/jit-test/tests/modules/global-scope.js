// Test interaction with global object and global lexical scope.

function parseAndEvaluate(source) {
    let m = parseModule(source);
    m.declarationInstantiation();
    return m.evaluation();
}

var x = 1;
assertEq(parseAndEvaluate("let r = x; x = 2; r"), 1);
assertEq(x, 2);

let y = 3;
assertEq(parseAndEvaluate("let r = y; y = 4; r"), 3);
assertEq(y, 4);

if (helperThreadCount() == 0)
    quit();

function offThreadParseAndEvaluate(source) {
    offThreadCompileModule(source);
    let m = finishOffThreadModule();
    print("compiled");
    m.declarationInstantiation();
    return m.evaluation();
}

assertEq(offThreadParseAndEvaluate("let r = x; x = 5; r"), 2);
assertEq(x, 5);

assertEq(offThreadParseAndEvaluate("let r = y; y = 6; r"), 4);
assertEq(y, 6);
