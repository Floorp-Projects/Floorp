// |jit-test| error: TypeError

"use strict";

setJitCompilerOption("baseline.warmup.trigger", 0);

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

registerModule('A', parseModule(ASrc));

let m = parseModule(mainSrc);
moduleLink(m)
moduleEvaluate(m);
