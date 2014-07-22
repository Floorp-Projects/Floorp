load(libdir + "asserts.js");
// Test ES6 Proxy trap compliance for Object.isExtensible() on exotic proxy
// objects.
var unsealed = {};
var sealed = Object.seal({});
var handler = {};

assertEq(Object.isExtensible(unsealed), true);
assertEq(Object.isExtensible(sealed), false);

var targetSealed = new Proxy(sealed, handler);
var targetUnsealed = new Proxy(unsealed, handler);

var handlerCalled = false;

function testExtensible(target, expectedResult, shouldIgnoreHandler = false)
{
    for (let p of [new Proxy(target, handler), Proxy.revocable(target, handler).proxy]) {
        handlerCalled = false;
        if (typeof expectedResult === "boolean")
            assertEq(Object.isExtensible(p), expectedResult, "Must return the correct value.");
        else
            assertThrowsInstanceOf(() => Object.isExtensible(p), expectedResult);
        assertEq(handlerCalled, !shouldIgnoreHandler, "Must call handler appropriately");
    }
}

// without traps, forward to the target
// First, make sure we get the obvious answer on a non-exotic target.
testExtensible(sealed, false, /* shouldIgnoreHandler = */true);
testExtensible(unsealed, true, /* shouldIgnoreHandler = */true);

// Now, keep everyone honest. What if the target itself is a proxy?
// Note that we cheat a little. |handlerCalled| is true in a sense, just not
// for the toplevel handler.
// While we're here, test that the argument is passed correctly.
var targetsTarget = {};
function ensureCalled(target) { assertEq(target, targetsTarget); handlerCalled = true; return true; }
var proxyTarget = new Proxy(targetsTarget, { isExtensible : ensureCalled });
testExtensible(proxyTarget, true);

// What if the trap says it's necessarily sealed?
function fakeSealed() { handlerCalled = true; return false; }
handler.isExtensible = fakeSealed;
testExtensible(targetSealed, false);
testExtensible(targetUnsealed, TypeError);

// What if the trap says it's never sealed?
function fakeUnsealed() { handlerCalled = true; return true; }
handler.isExtensible = fakeUnsealed;
testExtensible(targetSealed, TypeError);
testExtensible(targetUnsealed, true);

// make sure we are able to prevent further extensions mid-flight and throw if the
// hook tries to lie.
function makeSealedTruth(target) { handlerCalled = true; Object.preventExtensions(target); return false; }
function makeSealedLie(target) { handlerCalled = true; Object.preventExtensions(target); return true; }
handler.isExtensible = makeSealedTruth;
testExtensible({}, false);
handler.isExtensible = makeSealedLie;
testExtensible({}, TypeError);

// What if the trap doesn't directly return a boolean?
function falseyNonBool() { handlerCalled = true; return undefined; }
handler.isExtensible = falseyNonBool;
testExtensible(sealed, false);
testExtensible(unsealed, TypeError);

function truthyNonBool() { handlerCalled = true; return {}; }
handler.isExtensible = truthyNonBool;
testExtensible(sealed, TypeError);
testExtensible(unsealed, true);

// What if the trap throws?
function ExtensibleError() { }
ExtensibleError.prototype = new Error();
ExtensibleError.prototype.constructor = ExtensibleError;
function throwFromTrap() { throw new ExtensibleError(); }
handler.isExtensible = throwFromTrap;

// exercise some other code paths and make sure that they invoke the trap and
// can handle the ensuing error.
for (let p of [new Proxy(sealed, handler), Proxy.revocable(sealed, handler).proxy]) {
    assertThrowsInstanceOf(function () { Object.isExtensible(p); },
                           ExtensibleError, "Must throw if the trap does.");
    assertThrowsInstanceOf(function () { Object.isFrozen(p); },
                           ExtensibleError, "Must throw if the trap does.");
    assertThrowsInstanceOf(function () { Object.isSealed(p); },
                           ExtensibleError, "Must throw if the trap does.");
}

// What if the trap likes to re-ask old questions?
for (let p of [new Proxy(sealed, handler), Proxy.revocable(sealed, handler).proxy]) {
    handler.isExtensible = (() => Object.isExtensible(p));
    assertThrowsInstanceOf(() => Object.isExtensible(p),
                           InternalError, "Should allow and detect infinite recurison.");
}
