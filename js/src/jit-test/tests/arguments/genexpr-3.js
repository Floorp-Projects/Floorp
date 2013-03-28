// 'arguments' is lexically scoped in genexpr in toplevel let-block.

{
    let arguments = [];
    assertEq((arguments for (p in {a: 1})).next(), arguments);
}
