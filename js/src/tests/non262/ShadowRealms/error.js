// |reftest| shell-option(--enable-shadow-realms) skip-if(!xulRuntime.shell)

let sr = new ShadowRealm();

try {
    sr.evaluate("throw new Error('hi')");
    assertEq(true, false, "Should have thrown");
} catch (e) {
    assertEq(e instanceof TypeError, true, "Correct type of error")
    assertEq(/Error: hi/.test(e.message), true, "Should have included information from thrown error");
}

try {
    sr.evaluate("throw new Error('∂å∂')");
    assertEq(true, false, "Should have thrown");
} catch (e) {
    assertEq(e instanceof TypeError, true, "Correct type of error")
    assertEq(/Error: ∂å∂/.test(e.message), true, "Should have included information from thrown error, UTF-8 Pass through.");
}

try {
    sr.evaluate("throw {name: 'Hello', message: 'goodbye'}");
    assertEq(true, false, "Should have thrown");
} catch (e) {
    assertEq(e instanceof TypeError, true, "Correct type of error")
    assertEq(/uncaught exception: Object/.test(e.message), true, "Should get generic fillin message, non-string");
}

try {
    sr.evaluate("throw {name: 10, message: 11}");
    assertEq(true, false, "Should have thrown");
} catch (e) {
    assertEq(e instanceof TypeError, true, "Correct type of error")
    assertEq(/uncaught exception: Object/.test(e.message), true, "Should get generic fillin message, non-string");
}


try {
    sr.evaluate("throw { get name() { return 'holy'; }, get message() { return 'smokes' } }");
    assertEq(true, false, "Should have thrown");
} catch (e) {
    assertEq(e instanceof TypeError, true, "Correct type of error")
    assertEq(/uncaught exception: Object/.test(e.message), true, "Should get generic error message, getters");
}

// Wrapped Functions
try {
    var wrapped = sr.evaluate("() => { throw new Error('hi') }");
    assertEq(!!wrapped, true, "Wrapped created");
    wrapped();
    assertEq(true, false, "Should have thrown");
} catch (e) {
    assertEq(e instanceof TypeError, true, "Correct type of error")
    assertEq(/Error: hi/.test(e.message), true, "Should have included information from thrown error");
}

try {
    var wrapped = sr.evaluate("() => { throw new Error('∂å∂') } ");
    assertEq(!!wrapped, true, "Wrapped created");
    wrapped();
    assertEq(true, false, "Should have thrown");
} catch (e) {
    assertEq(e instanceof TypeError, true, "Correct type of error")
    assertEq(/Error: ∂å∂/.test(e.message), true, "Should have included information from thrown error, UTF-8 Pass through.");
}

try {
    var wrapped = sr.evaluate("() => { throw {name: 'Hello', message: 'goodbye'} } ");
    assertEq(!!wrapped, true, "Wrapped created");
    wrapped();
    assertEq(true, false, "Should have thrown");
} catch (e) {
    assertEq(e instanceof TypeError, true, "Correct type of error")
    assertEq(/uncaught exception: Object/.test(e.message), true, "Should get generic error message");
}

try {
    var wrapped = sr.evaluate("() =>  { throw {name: 10, message: 11} } ");
    assertEq(!!wrapped, true, "Wrapped created");
    wrapped();
    assertEq(true, false, "Should have thrown");
} catch (e) {
    assertEq(e instanceof TypeError, true, "Correct type of error")
    print(e.message)
    assertEq(/uncaught exception: Object/.test(e.message), true, "Should get generic error message");
}


try {
    var wrapped = sr.evaluate("() => {  throw { get name() { return 'holy'; }, get message() { return 'smokes' } } } ");
    assertEq(!!wrapped, true, "Wrapped created");
    wrapped();
    assertEq(true, false, "Should have thrown");
} catch (e) {
    assertEq(e instanceof TypeError, true, "Correct type of error")
    assertEq(/uncaught exception: Object/.test(e.message), true, "Should get generic error message");
}



if (typeof reportCompare === 'function')
    reportCompare(true, true);