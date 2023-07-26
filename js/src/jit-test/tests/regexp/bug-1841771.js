var lfLogBuffer = `
let hasFunction ;
for (const name of [, "" ]) 
  g55 = newGlobal();
gcparam('maxBytes', gcparam('gcBytes') );
//corefuzz-dcd-endofdata
/*
//corefuzz-dcd-endofdata
/**/



let hasFunction ;
for (const name of [ "", "", "", ""]) {





    const present = name in this;
    if (!present) 
        thisname = function() {};
}
//corefuzz-dcd-endofdata
//corefuzz-dcd-selectmode 1089924061
//corefuzz-dcd-endofdata
oomTest(function() {
  let m14 = parseModule('a'.replace(/a/, assertEq.toString));
});
`;
lfLogBuffer = lfLogBuffer.split('\n');
let lfPreamble = `
  Object.defineProperty(this, "fuzzutils", {
    value:{
        untemplate: function(s) {
          return s.replace(/\\\\/g, '\\\\\\\\').replace(/\`/g, '\\\\\`').replace(/\\\$/g, '\\\\\$'); 
        }
    }
  });
function lfFixRedeclaration(lfSrc, lfExc, lfRewriteSet) {
    let varName;
    let srcParts = lfSrc.split("\\n");
    let regReplace = new RegExp;
    for (let lfIdx = 0; lfIdx < srcParts.length; ++lfIdx)
      srcParts[lfExc.lineNumber - 1] = srcParts[lfExc.lineNumber - 1].replace(regReplace, varName);
    return srcParts.join();
}
`;
evaluate(lfPreamble);
let lfRunTypeId = -1;
let lfCodeBuffer = "";
while (true) {
    let line = lfLogBuffer.shift();
    if (line == null) break;
    else if (line == "//corefuzz-dcd-endofdata") {
        loadFile(lfCodeBuffer);
        lfCodeBuffer = "";
    } else lfCodeBuffer += line + "\n";
}
loadFile(lfCodeBuffer);
function loadFile(lfVarx, lfForceRunType = 0, lfPatchSets = []) {
    try {
        if (lfVarx.indexOf("//corefuzz-dcd-selectmode ") === 0) {
            if (lfGCStress);
        } else {
                evaluate(lfVarx);
        }
    } catch (lfVare) {
        if (lfVare.toString.indexOf0 >= 0);
        else if (lfVare.toString().indexOf("redeclaration of ") >= 0) {
            let lfNewSrc = lfFixRedeclaration(lfVarx, lfVare, lfPatchSets);
            loadFile(lfNewSrc, lfRunTypeId, lfPatchSets);
        }
        lfVarx = fuzzutils.untemplate(lfVarx);
    }
}
