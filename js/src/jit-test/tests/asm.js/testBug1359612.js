load(libdir + 'asm.js');

asmLink(asmCompile('stdlib', 'foreign', USE_ASM + `
  var ff = foreign.ff;
  function f() {
      ff(+1);
  }
  return f
`), this, { ff: Math.log1p });
