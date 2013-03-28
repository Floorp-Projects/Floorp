// 'arguments' is lexically scoped in genexpr in function.

function f() {
    assertEq((arguments for (x of [0])).next(),
             (arguments for (y of [1])).next());
}
f();
