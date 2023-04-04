// |jit-test| skip-if: !('oomTest' in this)

load(libdir + "asserts.js");

let stencil = compileToStencil('314;');
assertEq(evalStencil(stencil), 314);

stencil = compileToStencil('let o = { a: 42, b: 2718 }["b"]; o', { prepareForInstantiate: true });
assertEq(evalStencil(stencil), 2718);

assertThrowsInstanceOf(() => compileToStencil('let fail ='), SyntaxError);
assertThrowsInstanceOf(() => compileToStencil('42;', 42), Error);

oomTest(function() {
    compileToStencil('"hello stencil!";', { prepareForInstantiate: false });
});

oomTest(function() {
    compileToStencil('"hello stencil!";', { prepareForInstantiate: true });
});

// Modules
function compileAndEvaluateModule(script, options) {
    if (!options) {
        options = {};
    }
    options.module = true;
    let stencil = compileToStencil(script, options);
    let m = instantiateModuleStencil(stencil);
    moduleLink(m);
    moduleEvaluate(m);
    return m;
}

var m = compileAndEvaluateModule('export let r = 314;');
assertEq(getModuleEnvironmentValue(m, "r"), 314);

m = compileAndEvaluateModule('export let r = { a: 42, b: 2718 }["b"];', { prepareForInstantiate: true });
assertEq(getModuleEnvironmentValue(m, "r"), 2718);

assertThrowsInstanceOf(() => compileAndEvaluateModule('let fail ='), SyntaxError);
assertThrowsInstanceOf(() => compileToStencil('42;', 42), Error);

oomTest(function() {
    compileAndEvaluateModule('export let r = "hello stencil!";', { prepareForInstantiate: false });
});

oomTest(function() {
    compileAndEvaluateModule('export let r = "hello stencil!";', { prepareForInstantiate: true });
});
