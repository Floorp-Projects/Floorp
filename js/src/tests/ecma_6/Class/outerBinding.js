// |reftest| skip-if(!xulRuntime.shell)
//
// The above skip-if is because global lexicals aren't yet implemented. Remove
// that and the |evaluate| call below once they are.
//
// A class statement creates a mutable lexical outer binding.

var test = `
class Foo { constructor() { } }
assertEq(typeof Foo, \"function\");
Foo = 5;
assertEq(Foo, 5);

{
    class foo { constructor() { } }
    assertEq(typeof foo, \"function\");
    foo = 4;
    assertEq(foo, 4);
}

{
    class PermanentBinding { constructor() { } }
    delete PermanentBinding;
    // That...didn't actually work, right?
    assertEq(typeof PermanentBinding, "function");
}

{
    try {
        evaluate(\`class x { constructor () { } }
                   throw new Error("FAIL");
                   class y { constructor () { } }
                 \`);
    } catch (e if e instanceof Error) { }
    assertEq(typeof x, "function");
    assertEq(y, undefined, "Congrats, you fixed top-level lexical scoping! " +
                           "Please uncomment the tests below for the real test.");
    // assertThrowsInstanceOf(() => y, ReferenceError);
}

/*
===== UNCOMMENT ME WHEN ENABLING THE TEST ABOVE. =====
const globalConstant = 0;
var earlyError = true;
try {
    ieval("earlyError = false; class globalConstant { constructor() { } }");
} catch (e if e instanceof TypeError) { }
assertEq(earlyError, true);
*/

function strictEvalShadows() {
    "use strict";
    let x = 4;
    eval(\`class x { constructor() { } }
           assertEq(typeof x, "function");
         \`);
    assertEq(x, 4);
}
strictEvalShadows()
`;

if (classesEnabled())
    eval(test);

if (typeof reportCompare === "function")
    reportCompare(0, 0, "OK");
