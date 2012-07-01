/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

function checkConstruct(thing, buggy) {
    try {
        new thing();
        assertEq(0, 1, "not reached " + thing);
    } catch (e) {
        if (buggy)
            assertEq(String(e.message).indexOf("is not a constructor") === -1, false);
        else
            assertEq(String(e.message).indexOf("thing is not a constructor") === -1, false);
    }
}

var re = /aaa/
checkConstruct(re, false);

var boundFunctionPrototype = Function.prototype.bind();
checkConstruct(boundFunctionPrototype, true);

var boundBuiltin = Math.sin.bind();
checkConstruct(boundBuiltin, true);

/* We set the proxies construct trap to undefined,
 * so the call trap is used as constructor.
 */

var proxiedFunctionPrototype = Proxy.create({}, Function.prototype, undefined);
checkConstruct(proxiedFunctionPrototype, false);

var proxiedBuiltin = Proxy.create({}, parseInt, undefined);
checkConstruct(proxiedBuiltin, false);


if (typeof reportCompare == 'function')
    reportCompare(0, 0, "ok");
