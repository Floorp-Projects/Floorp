// |jit-test| skip-if: helperThreadCount() === 0 || isLcovEnabled()

const eagerOptions = {
  fileName: "certViewer.js",
  lineNumber: 1,
  eagerDelazificationStrategy: "OnDemandOnly",
};

const concurrentOptions = {
  fileName: "certViewer.js",
  lineNumber: 1,
  eagerDelazificationStrategy: "ConcurrentDepthFirst",
};

// Check that `undefined` is properly resolved to the global scope.
let script = `
function certDecoderInitializer() {
  return undefined;
}

let result = certDecoderInitializer();
export { result };
`;

function evalModule(source, opts) {
  let job = offThreadCompileModuleToStencil(source, opts);
  let stencil = finishOffThreadStencil(job);
  let m = instantiateModuleStencil(stencil);
  moduleLink(m);
  moduleEvaluate(m)
  return m.result;
}

assertEq(evalModule(script, eagerOptions), undefined);
assertEq(evalModule(script, concurrentOptions), undefined);
