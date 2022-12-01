// |jit-test| skip-if: helperThreadCount() === 0

offThreadCompileToStencil(`function outer() {
  // enough inner functions to make JS::Vector realloc
   let inner1 = () => {};
   let inner2 = () => {};
   let inner3 = () => {};
   let inner4 = () => {};
   let inner5 = () => {};
} `);
const stencil = finishOffThreadStencil();
