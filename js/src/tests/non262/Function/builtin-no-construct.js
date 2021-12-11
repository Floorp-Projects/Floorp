/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

function checkMethod(method) {
    try {
        new method();
        assertEq(0, 1, "not reached " + method);
    } catch (e) {
        assertEq(e.message.indexOf(" is not a constructor") === -1, false);
    }
}

function checkMethods(proto) {
    var names = Object.getOwnPropertyNames(proto);
    for (var i = 0; i < names.length; i++) {
        var name = names[i];
        if (["constructor", "arguments", "caller"].indexOf(name) >= 0)
            continue;
        var prop = proto[name];
        if (typeof prop === "function")
            checkMethod(prop);
    }
}

checkMethod(Function.prototype);
checkMethods(JSON);
checkMethods(Math);
checkMethods(Proxy);

var builtin_ctors = [
    Object, Function, Array, String, Boolean, Number, Date, RegExp, Error,
    EvalError, RangeError, ReferenceError, SyntaxError, TypeError, URIError,
];

for (var i = 0; i < builtin_ctors.length; i++) {
    checkMethods(builtin_ctors[i]);
    checkMethods(builtin_ctors[i].prototype);
}

var builtin_funcs = [
    eval, isFinite, isNaN, parseFloat, parseInt,
    decodeURI, decodeURIComponent, encodeURI, encodeURIComponent
];

for (var i = 0; i < builtin_funcs.length; i++) {
    checkMethod(builtin_funcs[i]);
}

if (typeof reportCompare == 'function')
    reportCompare(0, 0, "ok");
