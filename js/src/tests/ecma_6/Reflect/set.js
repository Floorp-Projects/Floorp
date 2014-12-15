if (this.Reflect && Reflect.set)
    throw new Error("Congrats on implementing Reflect.set! Uncomment this test for 1 karma point.");

/*
// Per spec, this should first call p.[[Set]]("0", 42, a) and
// then (since p has no own properties) a.[[Set]]("0", 42, a).
// Obviously the latter should not define a property on p.

var a = [0, 1, 2, 3];
var p = Object.create(a);
Reflect.set(p, "0", 42, a);
assertEq(p.hasOwnProperty("0"), false);
assertEq(a[0], 42);
*/

reportCompare(0, 0, 'ok');
