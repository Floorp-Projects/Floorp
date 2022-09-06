// |jit-test| --ion-eager; skip-if: helperThreadCount() === 0

// (1) Poison an element in the ionLazyLinkList with a builder whose
//     script is in a different compartment.
evaluate(`
offThreadCompileToStencil("var x = -1");
var stencil = finishOffThreadStencil();
evalStencil(stencil);
`,
         { global: newGlobal() });

// (2) Spam the ionLazyLinkList with pending builders until it pops off the one
//     for the other compartment's script.
for (var i = 0; i < 1000; ++i) {
  offThreadCompileToStencil('var x = ' + i);
  var stencil = finishOffThreadStencil();
  evalStencil(stencil);
}
