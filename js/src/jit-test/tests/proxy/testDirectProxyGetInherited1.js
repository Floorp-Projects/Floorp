// Getting a property that exists on an ordinary object
// does not touch a proxy on its proto chain.

load(libdir + "asserts.js");

var angryHandler = new Proxy({}, {
    get(t, id) { throw new Error("angryHandler should not be queried (" + id + ")"); }
});
var angryProto = new Proxy({}, angryHandler);
var obj = Object.create(angryProto, {
    x: {value: 3},
    y: {get: () => 4}
});
assertThrowsInstanceOf(() => obj.z, Error);  // check that angryProto works
assertEq(obj.x, 3);
assertEq(obj.y, 4);
