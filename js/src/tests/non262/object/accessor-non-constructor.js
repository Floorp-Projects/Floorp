var obj = { get a() { return 1; } };
assertThrowsInstanceOf(() => {
    new Object.getOwnPropertyDescriptor(obj, "a").get
}, TypeError);

obj = { set a(b) { } };
assertThrowsInstanceOf(() => {
    new Object.getOwnPropertyDescriptor(obj, "a").set
}, TypeError);

obj = { get a() { return 1; }, set a(b) { } };
assertThrowsInstanceOf(() => {
    new Object.getOwnPropertyDescriptor(obj, "a").get
}, TypeError);
assertThrowsInstanceOf(() => {
    new Object.getOwnPropertyDescriptor(obj, "a").set
}, TypeError);

if (typeof reportCompare === "function")
    reportCompare(true, true);
