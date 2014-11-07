// Assigning to a non-existing property of a plain object defines that
// property on that object, even if a proxy is on the proto chain.

// Create an object that behaves just like obj except it throws (instead of
// returning undefined) if you try to get a property that doesn't exist.
function throwIfNoSuchProperty(obj) {
    return new Proxy(obj, {
        get(t, id) {
            if (id in t)
                return t[id];
            throw new Error("no such handler method: " + id);
        }
    });
}

// Use a touchy object as our proxy handler in this test.
var hits = 0, savedDesc = undefined;
var touchyHandler = throwIfNoSuchProperty({
    set: undefined
});
var target = {};
var proto = new Proxy(target, touchyHandler);
var receiver = Object.create(proto);

// This assignment `receiver.x = 2` results in a series of [[Set]] calls,
// starting with:
//
// - receiver.[[Set]]()
//     - receiver is an ordinary object.
//     - This looks for an own property "x" on receiver. There is none.
//     - So it walks the prototype chain, doing a tail-call to:
// - proto.[[Set]]()
//     - proto is a proxy.
//     - This does handler.[[Get]]("set") to look for a set trap
//         (that's why we need `set: undefined` on the handler, above)
//     - Since there's no "set" handler, it tail-calls:
// - target.[[Set]]()
//     - ordinary
//     - no own property "x"
//     - tail call to:
// - Object.prototype.[[Set]]()
//     - ordinary
//     - no own property "x"
//     - We're at the end of the line: there's nothing left on the proto chain.
//     - So at last we call:
// - receiver.[[DefineOwnProperty]]()
//     - ordinary
//     - creates the property
//
// Got all that? Let's try it.
//
receiver.x = 2;
assertEq(receiver.x, 2);

var desc = Object.getOwnPropertyDescriptor(receiver, "x");
assertEq(desc.enumerable, true);
assertEq(desc.configurable, true);
assertEq(desc.writable, true);
assertEq(desc.value, 2);

