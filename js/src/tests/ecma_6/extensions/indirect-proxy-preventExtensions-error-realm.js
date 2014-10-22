// |reftest| skip-if(!xulRuntime.shell) -- needs newGlobal()
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var gTestfile = "indirect-proxy-preventExtensions-error-realm.js";
var BUGNUMBER = 1085566;
var summary =
  "The preventExtensions trap should return success/failure (with the " +
  "outermost preventExtension caller deciding what to do in response), " +
  "rather than throwing a TypeError itself";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var g = newGlobal();

// Someday indirect proxies will be thrown into the lake of fire and sulfur
// alongside the beast and the false prophet, to be tormented day and night for
// ever and ever.  When that happens, this test will need to be updated.
//
// The requirements for |p| are that 1) it be a cross-compartment wrapper, 2) to
// a proxy, 3) whose C++ preventExtensions handler method returns true while
// setting |*succeeded = false|.  The other options when this test was written)
// were a global scope polluter (WindowNamedPropertiesHandler), a window object
// (nsOuterWindowProxy), a DOM proxy (DOMProxyHandler), a SecurityWrapper, a
// DebugScopeProxy (seemingly never directly exposed to script), and an
// XrayWrapper.  Sadly none of these are simply available in both shell and
// browser, so indirect proxies are it.
//
// (The other option would be to do this with a jsapi-test that defines a custom
// proxy, but this was easier to write.  Maybe in the future, if no other such
// proxies arise before indirect proxies die.)
var p = g.Proxy.create({}, {});

try
{
  // What we expect to happen is this:
  //
  //   * Object.preventExtensions delegates to
  //   * cross-compartment wrapper preventExtensions trap, which delegates to
  //   * indirect proxy preventExtensions trap, which sets |*succeeded = false|
  //     *and does not throw or return false (in the JSAPI sense)*
  //
  // Returning false does not immediately create an error.  Instead that bubbles
  // backward through the layers to the initial [[PreventExtensions]] call, made
  // by Object.preventExtensions.  That function then creates and throws a
  // TypeError in response to that false result -- from its realm, not from the
  // indirect proxy's realm.
  Object.preventExtensions(p);

  throw new Error("didn't throw at all");
}
catch (e)
{
  assertEq(e instanceof TypeError, true,
           "expected a TypeError from this global, instead got " + e +
           ", from " +
           (e.constructor === TypeError
            ? "this global"
            : e.constructor === g.TypeError
            ? "the proxy's global"
            : "somewhere else (!!!)"));
}

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
