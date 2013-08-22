// obj.hasOwnProperty(id), Object.getOwnPropertyDescriptor(obj, id), and
// Object.defineProperty(obj, id, desc) do not look at obj's prototype.

var angryHandler = new Proxy({}, {
    has: () => true,
    get: (t, id) => {
        throw new Error("angryHandler should not be queried (" + id + ")");
    }
});
var angryProto = new Proxy({}, angryHandler);

var obj = Object.create(angryProto, {
    // Define hasOwnProperty directly on obj since we are poisoning its
    // prototype chain.
    hasOwnProperty: {
        value: Object.prototype.hasOwnProperty
    }
});

assertEq(Object.getOwnPropertyDescriptor(obj, "foo"), undefined);
assertEq(obj.hasOwnProperty("foo"), false);
Object.defineProperty(obj, "foo", {value: 5});
assertEq(obj.hasOwnProperty("foo"), true);
assertEq(obj.foo, 5);
