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

// without traps, forward to the target
// First, make sure we get the obvious answer on a non-exotic target.
assertEq(Object.isExtensible(targetSealed), false, "Must forward to target without hook.");
assertEq(Object.isExtensible(targetUnsealed), true, "Must forward to target without hook.");

// Now, keep everyone honest. What if the target itself is a proxy?
function ensureCalled() { handlerCalled = true; return true; }
var proxyTarget = new Proxy({}, { isExtensible : ensureCalled });
assertEq(Object.isExtensible(new Proxy(proxyTarget, {})), true, "Must forward to target without hook.");
assertEq(handlerCalled, true, "Must forward to target without hook.");

// with traps, makes sure that the handler is called, and that we throw if the
// trap disagrees with the target
function testExtensible(obj, shouldThrow, expectedResult)
{
    handlerCalled = false;
    if (shouldThrow)
        assertThrowsInstanceOf(function () { Object.isExtensible(obj); },
                               TypeError, "Must throw if handler and target disagree.");
    else
        assertEq(Object.isExtensible(obj), expectedResult, "Must return the correct value.");
    assertEq(handlerCalled, true, "Must call handler trap if present");
}

// What if the trap says it's necessarily sealed?
function fakeSealed() { handlerCalled = true; return false; }
handler.isExtensible = fakeSealed;
testExtensible(targetSealed, false, false);
testExtensible(targetUnsealed, true);

// What if the trap says it's never sealed?
function fakeUnsealed() { handlerCalled = true; return true; }
handler.isExtensible = fakeUnsealed;
testExtensible(targetSealed, true);
testExtensible(targetUnsealed, false, true);

// make sure we are able to prevent further extensions mid-flight and throw if the
// hook tries to lie.
function makeSealedTruth(target) { handlerCalled = true; Object.preventExtensions(target); return false; }
function makeSealedLie(target) { handlerCalled = true; Object.preventExtensions(target); return true; }
handler.isExtensible = makeSealedTruth;
testExtensible(new Proxy({}, handler), false, false);
handler.isExtensible = makeSealedLie;
testExtensible(new Proxy({}, handler), true);

// What if the trap doesn't directly return a boolean?
function falseyNonBool() { handlerCalled = true; return undefined; }
handler.isExtensible = falseyNonBool;
testExtensible(targetSealed, false, false);
testExtensible(targetUnsealed, true);

function truthyNonBool() { handlerCalled = true; return {}; }
handler.isExtensible = truthyNonBool;
testExtensible(targetSealed, true);
testExtensible(targetUnsealed, false, true);

// What if the trap throws?
function ExtensibleError() { }
ExtensibleError.prototype = new Error();
ExtensibleError.prototype.constructor = ExtensibleError;
function throwFromTrap() { throw new ExtensibleError(); }
handler.isExtensible = throwFromTrap;

// exercise some other code paths and make sure that they invoke the trap and
// can handle the ensuing error.
assertThrowsInstanceOf(function () { Object.isExtensible(targetSealed); },
                       ExtensibleError, "Must throw if the trap does.");
assertThrowsInstanceOf(function () { Object.isFrozen(targetSealed); },
                       ExtensibleError, "Must throw if the trap does.");
assertThrowsInstanceOf(function () { Object.isSealed(targetSealed); },
                       ExtensibleError, "Must throw if the trap does.");


// What if the trap likes to re-ask old questions?
function recurse() { return Object.isExtensible(targetSealed); }
handler.isExtensible = recurse;
assertThrowsInstanceOf(function () { Object.isExtensible(targetSealed); },
                       InternalError, "Should allow and detect infinite recurison.");

reportCompare(0, 0, "OK");
