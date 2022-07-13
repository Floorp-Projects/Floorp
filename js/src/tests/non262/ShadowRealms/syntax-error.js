// |reftest| shell-option(--enable-shadow-realms) skip-if(!xulRuntime.shell)

let sr = new ShadowRealm();

try {
    sr.evaluate("var x /");
    assertEq(true, false, "Should have thrown");
} catch (e) {
    assertEq(e instanceof SyntaxError, true, "Same Global Error")
    assertEq(/unterminated regular expression literal/.test(e.message), true, "Should have reported a sensible error message");
}

try {
    sr.evaluate("var x =");
    assertEq(true, false, "Should have thrown");
} catch (e) {
    assertEq(e instanceof SyntaxError, true, "Same Global Error")
    assertEq(/expected expression/.test(e.message), true, "Should have reported a sensible error message");
}


try {
    sr.evaluate("#x in this");
    assertEq(true, false, "Should have thrown");
} catch (e) {
    assertEq(e instanceof SyntaxError, true, "Same Global Error")
    assertEq(/reference to undeclared private field or method/.test(e.message), true, "Should have reported a sensible error message");
}



if (typeof reportCompare === 'function')
    reportCompare(true, true);