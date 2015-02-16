/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

function checkConstruct(thing) {
    try {
        new thing();
        assertEq(0, 1, "not reached " + thing);
    } catch (e) {
        assertEq(String(e.message).indexOf(" is not a constructor") === -1, false);
    }
}

var re = /aaa/
checkConstruct(re);

var boundFunctionPrototype = Function.prototype.bind();
checkConstruct(boundFunctionPrototype);

var boundBuiltin = Math.sin.bind();
checkConstruct(boundBuiltin);

/* We set the proxies construct trap to undefined,
 * so the call trap is used as constructor.
 */

var handler = {
    getPropertyDescriptor(name) {
        /* toSource may be called to generate error message. */
        assertEq(name, "toSource");
        return { value: () => "foo" };
    }
};

var proxiedFunctionPrototype = Proxy.create(handler, Function.prototype, undefined);
checkConstruct(proxiedFunctionPrototype);

var proxiedBuiltin = Proxy.create(handler, parseInt, undefined);
checkConstruct(proxiedBuiltin);


if (typeof reportCompare == 'function')
    reportCompare(0, 0, "ok");
