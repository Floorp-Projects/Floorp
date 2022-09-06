// |jit-test| skip-if: helperThreadCount() === 0

load(libdir + "asserts.js");

offThreadCompileToStencil("var shouldFailToParse =");

assertThrowsInstanceOf(() => finishOffThreadStencil(), SyntaxError);
