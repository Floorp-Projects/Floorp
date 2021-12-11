/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function logProxy(object = {}, handler = {}) {
    var log = [];
    var proxy = new Proxy(object, new Proxy(handler, {
        get(target, propertyKey, receiver) {
            log.push(propertyKey);
            return target[propertyKey];
        }
    }));
    return {proxy, log};
}

// The order of operations is backwards when compared to ES6 draft rev 27
// (2014 August 24), but see https://bugs.ecmascript.org/show_bug.cgi?id=3215
// for an explanation on why the spec version is clearly wrong.

var {proxy, log} = logProxy();
Object.seal(proxy);
assertDeepEq(log, ["preventExtensions", "ownKeys"]);

var {proxy, log} = logProxy();
Object.seal(Object.seal(proxy));
assertDeepEq(log, ["preventExtensions", "ownKeys", "preventExtensions", "ownKeys"]);

reportCompare(0, 0);
