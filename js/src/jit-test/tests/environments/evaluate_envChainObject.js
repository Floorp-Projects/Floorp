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

// Unqualified variables objects are not allowed.

if (!isProxy(evalcx(""))) {
  // if --more-compartments option is not give, evalcx returns sandbox,
  // which is unqualified variables object.
  assertThrowsInstanceOf(() => {
    evaluate("10", { envChainObject: evalcx("") });
  }, Error);
}

// evalReturningScope returns NonSyntacticVariablesObject, which is unqualified
// variables object.
assertThrowsInstanceOf(() => {
  evaluate("10", { envChainObject: evalReturningScope("1") });
}, Error);
