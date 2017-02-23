var lfLogBuffer = `
gczeal(15,10);
try {
    a = []
    gczeal(2, 2)()
} catch (e) {}
a.every(function() {})
//corefuzz-dcd-endofdata
//corefuzz-dcd-selectmode 5
`;
lfLogBuffer = lfLogBuffer.split('\n');
lfPreamble = `
`;
var lfCodeBuffer = "";
var lfRunTypeLimit = 7;
var lfOffThreadGlobal = newGlobal();
try {} catch (lfVare5) {}
var lfAccumulatedCode = lfPreamble;
while (true) {
    var line = lfLogBuffer.shift();
    if (line == null) {
        break;
    } else if (line == "//corefuzz-dcd-endofdata") {
        loadFile(lfCodeBuffer);
    } else if (line.indexOf("//corefuzz-dcd-selectmode ") === 0) {
        loadFile(line);
    } else {
        lfCodeBuffer += line + "\n";
    }
}
if (lfCodeBuffer) loadFile(lfCodeBuffer);
function loadFile(lfVarx) {
    try {
        if (lfVarx.indexOf("//corefuzz-dcd-selectmode ") === 0) {
            lfRunTypeId = parseInt(lfVarx.split(" ")[1]) % lfRunTypeLimit;
        } else {
            switch (lfRunTypeId) {
                case 5:
                    evalInWorker(lfAccumulatedCode);
                    evaluate(lfVarx);
            }
        }
    } catch (lfVare) {
        lfAccumulatedCode += "try { evaluate(`\n" + lfVarx + "\n`); } catch(exc) {}\n";
    }
}
