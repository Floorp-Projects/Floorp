// |jit-test| skip-if: !('oomTest' in this)

lfLogBuffer = `
  let c = registerModule('c', parseModule(""));
  let d = registerModule('d', parseModule("import { a } from 'c'; a;"));
  moduleLink(d);
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
            moduleLink(m);
            moduleEvaluate(m);
        });
    } catch (lfVare) {}
}
