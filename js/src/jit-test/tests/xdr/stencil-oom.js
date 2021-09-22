// |jit-test| skip-if: !('oomTest' in this)

const sa = `
function f(x, y) { return x + y }
let a = 10, b = 20;
f(a, b);
`;

oomTest(() => {
    let stencil = compileToStencilXDR(sa);
    evalStencilXDR(stencil);
});
