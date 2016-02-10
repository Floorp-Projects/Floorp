// Test off thread module compilation.

if (helperThreadCount() == 0)
    quit();

load(libdir + "dummyModuleResolveHook.js");

function offThreadParse(source) {
    offThreadCompileModule(source);
    return finishOffThreadModule();
}

const sa =
`export default 20;
 export let a = 22;
 export function f(x, y) { return x + y }
`;

const sb =
`import x from "a";
 import { a as y } from "a";
 import * as ns from "a";
 ns.f(x, y);
`;

let a = moduleRepo['a'] = offThreadParse(sa);
let b = moduleRepo['b'] = offThreadParse(sb);
b.declarationInstantiation();
assertEq(b.evaluation(), 42);

