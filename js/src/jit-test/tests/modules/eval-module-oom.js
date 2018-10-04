// OOM tests for module parsing.

if (!('oomTest' in this))
    quit();

load(libdir + "dummyModuleResolveHook.js");

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

oomTest(() => {
    let a = moduleRepo['a'] = parseModule(sa);
    let b = moduleRepo['b'] = parseModule(sb);
    b.declarationInstantiation();
    assertEq(b.evaluation(), 42);
});
