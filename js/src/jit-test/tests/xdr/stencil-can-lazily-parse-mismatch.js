// |jit-test| skip-if: isLcovEnabled()
// Code coverage forces full-parse.

load(libdir + 'asserts.js');

const code = `var a = 10;`;

function testCompile(sourceIsLazy1, sourceIsLazy2,
                     forceFullParse1, forceFullParse2) {
  const stencil = compileToStencil(code, { sourceIsLazy: sourceIsLazy1,
                                           forceFullParse: forceFullParse1 });
  assertThrowsInstanceOf(() => {
    evalStencil(stencil, { sourceIsLazy: sourceIsLazy2,
                           forceFullParse: forceFullParse2 });
  }, Error);
}

function testOffThreadCompile(sourceIsLazy1, sourceIsLazy2,
                              forceFullParse1, forceFullParse2) {
  offThreadCompileToStencil(code, { sourceIsLazy: sourceIsLazy1,
                                    forceFullParse: forceFullParse1 });
  const stencil = finishOffThreadCompileToStencil();
  assertThrowsInstanceOf(() => {
    evalStencil(stencil, { sourceIsLazy: sourceIsLazy2,
                           forceFullParse: forceFullParse2 });
  }, Error);
}

function testXDR(sourceIsLazy1, sourceIsLazy2,
                 forceFullParse1, forceFullParse2) {
  const xdr = compileToStencilXDR(code, { sourceIsLazy: sourceIsLazy1,
                                          forceFullParse: forceFullParse1 });
  // The compile options are ignored when decoding, and no error is thrown.
  evalStencilXDR(xdr, { sourceIsLazy: sourceIsLazy2,
                        forceFullParse: forceFullParse2 });
}

function testOffThreadXDR(sourceIsLazy1, sourceIsLazy2,
                          forceFullParse1, forceFullParse2) {
  const t = cacheEntry(code);
  evaluate(t, { sourceIsLazy: sourceIsLazy1,
                forceFullParse: forceFullParse1,
                saveIncrementalBytecode: true });
  offThreadDecodeScript(t, { sourceIsLazy: sourceIsLazy2,
                             forceFullParse: forceFullParse2 });
  // The compile options are ignored when decoding, and no error is thrown.
  runOffThreadDecodedScript();
}

const optionsList = [
  [true, false, false, false],
  [false, true, false, false],
  [false, false, true, false],
  [false, false, false, true],
  [true, false, true, false],
  [false, true, false, true],
];

for (const options of optionsList) {
  testCompile(...options);
  if (helperThreadCount() > 0) {
    testOffThreadCompile(...options);
  }
  testXDR(...options);
  if (helperThreadCount() > 0) {
    testOffThreadXDR(...options);
  }
}
