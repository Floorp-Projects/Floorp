// |jit-test| skip-if: isLcovEnabled()

load(libdir + "asserts.js");

const gWithSource = newGlobal({discardSource: false});
const gWithoutSource = newGlobal({discardSource: true});

const stencil = compileToStencil("");

gWithSource.evalStencil(stencil);
assertThrowsInstanceOf(() => gWithoutSource.evalStencil(stencil), gWithoutSource.Error);

const xdr = compileToStencilXDR("");
gWithSource.evalStencilXDR(xdr);
assertThrowsInstanceOf(() => gWithoutSource.evalStencilXDR(xdr), gWithoutSource.Error);

const code = cacheEntry("");

evaluate(code, { global: gWithSource, saveIncrementalBytecode: true});
evaluate(code, { global: gWithSource, loadBytecode: true});
assertThrowsInstanceOf(() => evaluate(code, { global: gWithoutSource, loadBytecode: true}), gWithoutSource.Error);

const moduleStencil = compileToStencil("", { module: true });
gWithSource.instantiateModuleStencil(moduleStencil);
assertThrowsInstanceOf(() => gWithoutSource.instantiateModuleStencil(moduleStencil), gWithoutSource.Error);

const moduleXDR = compileToStencilXDR("", { module: true });
gWithSource.instantiateModuleStencilXDR(moduleXDR);
assertThrowsInstanceOf(() => gWithoutSource.instantiateModuleStencilXDR(moduleXDR), gWithoutSource.Error);
