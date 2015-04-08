function f(x) {
    Object.defineProperty(arguments, 0, {
        get: function() {}
    });
    return arguments;
}

var obj = f(1);
assertEq(obj[0], undefined);
assertEq(Object.getOwnPropertyDescriptor(obj, 0).set, undefined);
assertThrowsInstanceOf(() => { "use strict"; obj[0] = 1; }, TypeError);

reportCompare(0, 0);
