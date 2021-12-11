// Test with envChainObject in current global.
{
    let global = newGlobal();
    let envChainObject = { a: "test1" };
    assertEq(evaluate("a", { global, envChainObject }), "test1");
}

// Test with envChainObject in target global.
{
    let global = newGlobal();
    var envChainObject = global.evaluate('({a: "test2"})');
    assertEq(envChainObject.a, "test2");
    assertEq(evaluate("a", { global, envChainObject }), "test2");
}
