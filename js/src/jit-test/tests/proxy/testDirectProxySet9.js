// Assigning to a proxy with no set handler calls the defineProperty handler
// when an existing own data property already exists on the target.

var t = {x: 1};
var p = new Proxy(t, {
    defineProperty(t, id, desc) {
        hits++;

        // ES6 draft rev 28 (2014 Oct 14) 9.1.9 step 5.e.i.
        // Since the property already exists, the system only changes
        // the value. desc is otherwise empty.
        assertEq(Object.getOwnPropertyNames(desc).join(","), "value");
        assertEq(desc.value, 42);
        return true;
    }
});
var hits = 0;
p.x = 42;
assertEq(hits, 1);
assertEq(t.x, 1);
