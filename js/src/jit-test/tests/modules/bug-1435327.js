// |jit-test| skip-if: !('oomTest' in this)

lfLogBuffer = `
  let moduleRepo = {};
  setModuleResolveHook(function(x, specifier) {
        return moduleRepo[specifier];
  });
  let c = moduleRepo['c'] = parseModule("");
  let d = moduleRepo['d'] = parseModule("import { a } from 'c'; a;");
  d.declarationInstantiation();
`;
lfLogBuffer = lfLogBuffer.split('\n');
var lfCodeBuffer = "";
while (true) {
    var line = lfLogBuffer.shift();
    if (line == null) {
        break;
    } else {
        lfCodeBuffer += line + "\n";
    }
}
if (lfCodeBuffer) loadFile(lfCodeBuffer);
function loadFile(lfVarx) {
    try {
        oomTest(function() {
            let m = parseModule(lfVarx);
            m.declarationInstantiation();
            m.evaluation();
        });
    } catch (lfVare) {}
}
