// Test interaction with global object and global lexical scope.

function evalModuleAndCheck(source, expected) {
    let m = parseModule(source);
    moduleLink(m);
    moduleEvaluate(m);
    assertEq(getModuleEnvironmentValue(m, "r"), expected);
}

var x = 1;
evalModuleAndCheck("export let r = x; x = 2;", 1);
assertEq(x, 2);

let y = 3;
evalModuleAndCheck("export let r = y; y = 4;", 3);
assertEq(y, 4);

if (helperThreadCount() == 0)
    quit();

function offThreadEvalModuleAndCheck(source, expected) {
    offThreadCompileModuleToStencil(source);
    let stencil = finishOffThreadStencil();
    let m = instantiateModuleStencil(stencil);
    print("compiled");
    moduleLink(m);
    moduleEvaluate(m);
    assertEq(getModuleEnvironmentValue(m, "r"), expected);
}

offThreadEvalModuleAndCheck("export let r = x; x = 5;", 2);
assertEq(x, 5);

offThreadEvalModuleAndCheck("export let r = y; y = 6;", 4);
assertEq(y, 6);
