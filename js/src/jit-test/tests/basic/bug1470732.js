// |jit-test| skip-if: helperThreadCount() === 0
var i = 0;
while(i++ < 500) {
  evalInWorker(`
    assertFloat32(0x23456789 | 0, false);
  `);
  let m = parseModule("");
  moduleLink(m);
}

