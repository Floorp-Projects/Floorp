setImmutablePrototype(Function.__proto__);

if (helperThreadCount() > 0) {
    offThreadCompileToStencil("function f() {}");
    var stencil = finishOffThreadCompileToStencil();
    evalStencil(stencil);
}
