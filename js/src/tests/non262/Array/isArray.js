/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/ */

var global = this;
var otherGlobal = newGlobal();

var thisGlobal = () => global;
var alternateGlobals = (function(i) {
    return () => (i++ % 2) === 0 ? global : otherGlobal;
})(0);

function performTests(pickGlobal)
{
    // Base case.
    assertEq(Array.isArray([]), true);

    // Simple case: proxy to an array.
    var proxy = new (pickGlobal()).Proxy([], {});
    assertEq(Array.isArray(proxy), true);

    // Recursive proxy ultimately terminating in an array.
    for (var i = 0; i < 10; i++) {
        proxy = new (pickGlobal()).Proxy(proxy, {});
        assertEq(Array.isArray(proxy), true);
    }

    // Revocable proxy to an array.
    var revocable = (pickGlobal()).Proxy.revocable([], {});
    proxy = revocable.proxy;
    assertEq(Array.isArray(proxy), true);

    // Recursive proxy ultimately terminating in a revocable proxy to an array.
    for (var i = 0; i < 10; i++) {
        proxy = new (pickGlobal()).Proxy(proxy, {});
        assertEq(Array.isArray(proxy), true);
    }

    // Revoked proxy to (formerly) an array.
    revocable.revoke();
    assertThrowsInstanceOf(() => Array.isArray(revocable.proxy), TypeError);

    // Recursive proxy ultimately terminating in a revoked proxy to an array.
    assertThrowsInstanceOf(() => Array.isArray(proxy), TypeError);

}

performTests(thisGlobal);
performTests(alternateGlobals);

function crossGlobalTest()
{
    var array = new otherGlobal.Array();

    // Array from another global.
    assertEq(Array.isArray(array), true);

    // Proxy to an array from another global.
    assertEq(Array.isArray(new Proxy(array, {})), true);

    // Other-global proxy to an array from that selfsame global.
    assertEq(Array.isArray(new otherGlobal.Proxy(array, {})), true);
}

crossGlobalTest();

if (typeof reportCompare === "function")
    reportCompare(true, true);
