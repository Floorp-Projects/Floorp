// |jit-test| skip-if: !('oomTest' in this)

function parseModule(source) {
  offThreadCompileModuleToStencil(source);
  var stencil = finishOffThreadStencil();
  return instantiateModuleStencil(stencil);
}
function loadFile(lfVarx) {
  oomTest(function() {
      parseModule(lfVarx);
  });
}
loadFile(`
const numberingSystems = {
  "adlm": {},
  "ahom": {},
  "arab": {},
  "arabext": {},
  "armn": {},
  "armnlow": {},
  "bali": {},
  "beng": {},
  "bhks": {},
  "brah": {},
  "cakm": {},
  "cham": {},
  "cyrl": {},
  "hmnp": {},
  "java": {},
  "jpan": {},
  "jpanfin": {},
  "jpanyear": {},
  "knda": {},
  "lana": {},
  "latn": {},
  "lepc": {},
  "limb": {},
  "wcho": {},
  "extra1": {}
};
`);
