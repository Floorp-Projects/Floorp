// ES6 7.1.1 ToPrimitive(input [, PreferredType]) specifies a new extension
// point in the language. Objects can override the behavior of ToPrimitive
// somewhat by supporting the method obj[@@toPrimitive](hint).
//
// (Rationale: ES5 had a [[DefaultValue]] internal method, overridden only by
// Date objects. The change in ES6 is to make [[DefaultValue]] a plain old
// method. This allowed ES6 to eliminate the [[DefaultValue]] internal method,
// simplifying the meta-object protocol and thus proxies.)

// obj[Symbol.toPrimitive]() is called whenever the ToPrimitive algorithm is invoked.
var expectedThis, expectedHint;
var obj = {
    [Symbol.toPrimitive](hint, ...rest) {
        assertEq(this, expectedThis);
        assertEq(hint, expectedHint);
        assertEq(rest.length, 0);
        return 2015;
    }
};
expectedThis = obj;
expectedHint = "string";
assertEq(String(obj), "2015");
expectedHint = "number";
assertEq(Number(obj), 2015);

// It is called even through proxies.
var proxy = new Proxy(obj, {});
expectedThis = proxy;
expectedHint = "default";
assertEq("ES" + proxy, "ES2015");

// It is called even through additional proxies and the prototype chain.
proxy = new Proxy(Object.create(proxy), {});
expectedThis = proxy;
expectedHint = "default";
assertEq("ES" + (proxy + 1), "ES2016");

// It is not called if the operand is already a primitive.
var ok = true;
for (var constructor of [Boolean, Number, String, Symbol]) {
    constructor.prototype[Symbol.toPrimitive] = function () {
        ok = false;
        throw "FAIL";
    };
}
assertEq(Number(true), 1);
assertEq(Number(77.7), 77.7);
assertEq(Number("123"), 123);
assertThrowsInstanceOf(() => Number(Symbol.iterator), TypeError);
assertEq(String(true), "true");
assertEq(String(77.7), "77.7");
assertEq(String("123"), "123");
assertEq(String(Symbol.iterator), "Symbol(Symbol.iterator)");
assertEq(ok, true);

// Converting a primitive symbol to another primitive type throws even if you
// delete the @@toPrimitive method from Symbol.prototype.
delete Symbol.prototype[Symbol.toPrimitive];
var sym = Symbol("ok");
assertThrowsInstanceOf(() => `${sym}`, TypeError);
assertThrowsInstanceOf(() => Number(sym), TypeError);
assertThrowsInstanceOf(() => "" + sym, TypeError);

// However, having deleted that method, converting a Symbol wrapper object does
// work: it calls Symbol.prototype.toString().
obj = Object(sym);
assertEq(String(obj), "Symbol(ok)");
assertEq(`${obj}`, "Symbol(ok)");

// Deleting valueOf as well makes numeric conversion also call toString().
delete Symbol.prototype.valueOf;
delete Object.prototype.valueOf;
assertEq(Number(obj), NaN);
Symbol.prototype.toString = function () { return "2060"; };
assertEq(Number(obj), 2060);

// Deleting Date.prototype[Symbol.toPrimitive] changes the result of addition
// involving Date objects.
var d = new Date;
assertEq(0 + d, 0 + d.toString());
delete Date.prototype[Symbol.toPrimitive];
assertEq(0 + d, 0 + d.valueOf());

// If @@toPrimitive, .toString, and .valueOf are all missing, we get a
// particular sequence of property accesses, followed by a TypeError exception.
var log = [];
function doGet(target, propertyName, receiver) {
    log.push(propertyName);
}
var handler = new Proxy({}, {
    get(target, trapName, receiver) {
        if (trapName !== "get")
            throw `FAIL: system tried to access handler method: ${uneval(trapName)}`;
        return doGet;
    }
});
proxy = new Proxy(Object.create(null), handler);
assertThrowsInstanceOf(() => proxy == 0, TypeError);
assertDeepEq(log, [Symbol.toPrimitive, "valueOf", "toString"]);

reportCompare(0, 0);
