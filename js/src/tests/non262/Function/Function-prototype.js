var desc = Object.getOwnPropertyDescriptor(Function.prototype, "length");
assertDeepEq(desc,
    {value: 0, writable: false, enumerable: false, configurable: true});

assertEq(Function.prototype.prototype, undefined);
assertEq(Function.prototype.callee, undefined);
assertThrowsInstanceOf(() => Function.prototype.caller, TypeError);
assertThrowsInstanceOf(() => Function.prototype.arguments, TypeError);

if (typeof reportCompare === "function")
    reportCompare(true, true);
