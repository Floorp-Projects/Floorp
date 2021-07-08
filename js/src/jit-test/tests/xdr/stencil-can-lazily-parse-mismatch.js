load(libdir + 'asserts.js');

const code = `var a = 10;`;

function testXDR(sourceIsLazy1, sourceIsLazy2,
                 forceFullParse1, forceFullParse2) {
  const xdr = compileToStencilXDR(code, { sourceIsLazy: sourceIsLazy1,
                                          forceFullParse: forceFullParse1 });
  assertThrowsInstanceOf(() => {
    evalStencilXDR(xdr, { sourceIsLazy: sourceIsLazy2,
                          forceFullParse: forceFullParse2 });
  }, Error);
}

function testOffThreadXDR(sourceIsLazy1, sourceIsLazy2,
                          forceFullParse1, forceFullParse2) {
  const t = cacheEntry(code);
  evaluate(t, { sourceIsLazy: sourceIsLazy1,
                forceFullParse: forceFullParse1,
                saveIncrementalBytecode: true });
  offThreadDecodeScript(t, { sourceIsLazy: sourceIsLazy2,
                             forceFullParse: forceFullParse2 });
  assertThrowsInstanceOf(() => {
    runOffThreadDecodedScript();
  }, Error);
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
  testXDR(...options);
  if (helperThreadCount() > 0) {
    testOffThreadXDR(...options);
  }
}
