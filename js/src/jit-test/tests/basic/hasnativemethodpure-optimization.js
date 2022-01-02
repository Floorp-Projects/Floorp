load(libdir + "asserts.js");

let string = Object.defineProperty(new String("123"), "valueOf", {
    get: function() { throw "get-valueOf"; }
});
assertThrowsValue(() => "" + string, "get-valueOf");

string = Object.defineProperty(new String("123"), "toString", {
    get: function() { throw "get-toString"; }
});
assertThrowsValue(() => string.toLowerCase(), "get-toString");

string = Object.defineProperty(new String("123"), Symbol.toPrimitive, {
    get: function() { throw "get-toPrimitive"; }
});
assertThrowsValue(() => string.toLowerCase(), "get-toPrimitive");

let number = Object.defineProperty(new Number(123), "valueOf", {
    get: function() { throw "get-valueOf"; }
});
assertThrowsValue(() => +number, "get-valueOf");