// |jit-test| error: TypeError

"use strict";

setJitCompilerOption("baseline.warmup.trigger", 0);

let moduleRepo = {};
setModuleResolveHook(function(module, specifier) {
    if (specifier in moduleRepo)
        return moduleRepo[specifier];
    throw "Module '" + specifier + "' not found";
});

let mainSrc = `
import A from "A";

const a = A;

function requestAnimationFrame(f) { Promise.resolve().then(f); };

requestAnimationFrame(loopy);
a = 2;
function loopy() {
    A;
}
`;

let ASrc = `
export default 1;
`;

moduleRepo['A'] = parseModule(ASrc);

let m = parseModule(mainSrc);
m.declarationInstantiation()
m.evaluation();
