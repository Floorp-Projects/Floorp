/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/ */

if (typeof Symbol === "function") {
    // for-in loops skip properties with symbol keys, even enumerable properties.
    var obj = {};
    obj[Symbol.for("moon")] = "sun";
    obj[Symbol("asleep")] = "awake";
    obj[Symbol.iterator] = "List";
    for (var x in obj)
        throw "FAIL: " + uneval(x);

    // This includes inherited properties.
    var obj2 = Object.create(obj);
    for (var x in obj2)
        throw "FAIL: " + uneval(x);

    // The same goes for proxies.
    var p = new Proxy(obj, {});
    for (var x in p)
        throw "FAIL: " + uneval(x);
    var p2 = new Proxy(obj2, {});
    for (var x in p2)
        throw "FAIL: " + uneval(x);

    // Object.keys() and .getOwnPropertyNames() also skip symbols.
    assertEq(Object.keys(obj).length, 0);
    assertEq(Object.keys(p).length, 0);
    assertEq(Object.keys(obj2).length, 0);
    assertEq(Object.keys(p2).length, 0);
    assertEq(Object.getOwnPropertyNames(obj).length, 0);
    assertEq(Object.getOwnPropertyNames(p).length, 0);
    assertEq(Object.getOwnPropertyNames(obj2).length, 0);
    assertEq(Object.getOwnPropertyNames(p2).length, 0);

    // Test interaction of Object.keys(), proxies, and symbol property keys.
    var log = [];
    var h = {
        ownKeys: (t) => {
            log.push("ownKeys");
            return ["a", "0", Symbol.for("moon"), Symbol("asleep"), Symbol.iterator];
        },
        getOwnPropertyDescriptor: (t, key) => {
            log.push("gopd", key);
            return {configurable: true, enumerable: true, value: 0, writable: true};
        }
    };
    p = new Proxy({}, h);
    assertDeepEq(Object.keys(p), ["a", "0"]);
    assertDeepEq(log, ["ownKeys", "gopd", "a", "gopd", "0"]);
}

if (typeof reportCompare === "function")
    reportCompare(0, 0);
