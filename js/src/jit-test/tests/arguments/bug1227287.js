// |jit-test| --no-warp
// Ion/Warp has known issues with function.arguments. See bug 1626294.

function f(y) {
    y = 1;
    assertEq(arguments.callee.arguments[0], 1);
    return () => y;
}
f(0);
