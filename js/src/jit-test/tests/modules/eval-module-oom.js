// |jit-test| skip-if: !('oomTest' in this)

// OOM tests for module parsing.

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
    let a = registerModule('a', parseModule(sa));
    let b = registerModule('b', parseModule(sb));
    moduleLink(b);
    assertEq(moduleEvaluate(b), 42);
});
