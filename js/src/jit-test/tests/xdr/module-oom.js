// |jit-test| skip-if: !('oomTest' in this)

// OOM tests for xdr module parsing.

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
    let stencil = compileToStencilXDR(sa, {module: true});
    let m = instantiateModuleStencilXDR(stencil);
    let a = registerModule('a', m);

    stencil = compileToStencilXDR(sb, {module: true});
    m = instantiateModuleStencilXDR(stencil);
    let b = registerModule('b', m);
});
