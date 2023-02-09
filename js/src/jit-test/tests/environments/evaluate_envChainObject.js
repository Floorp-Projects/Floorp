load(libdir + "asserts.js");

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

// Unqualified variable objects are not allowed.
assertThrowsInstanceOf(() => {
    evaluate("10", { envChainObject: evalcx("") });
}, Error);

assertThrowsInstanceOf(() => {
    evaluate("10", { envChainObject: evalReturningScope("1") });
}, Error);
