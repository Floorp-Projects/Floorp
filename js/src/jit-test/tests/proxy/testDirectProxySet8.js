// Assigning to a proxy with no set handler calls the defineProperty handler
// when an existing inherited data property already exists.

var hits = 0;
var proto = {x: 1};
var t = Object.create(proto);
var p = new Proxy(t, {
    defineProperty(t, id, desc) { hits++; return true; }
});
p.x = 2;
assertEq(hits, 1);
assertEq(proto.x, 1);
assertEq(t.hasOwnProperty("x"), false);

// Same thing, but the receiver is a plain object inheriting from p:
var receiver = Object.create(p);
receiver.x = 2;
assertEq(hits, 1);
assertEq(t.hasOwnProperty("x"), false);
assertEq(receiver.hasOwnProperty("x"), true);
assertEq(receiver.x, 2);
