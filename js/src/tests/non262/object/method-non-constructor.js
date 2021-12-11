var obj = { method() { } };
assertThrowsInstanceOf(() => {
    new obj.method;
}, TypeError);

obj = { constructor() { } };
assertThrowsInstanceOf(() => {
    new obj.constructor;
}, TypeError);

if (typeof reportCompare === "function")
    reportCompare(true, true);
