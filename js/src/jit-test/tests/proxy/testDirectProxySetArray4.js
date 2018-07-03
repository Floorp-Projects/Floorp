// Assigning to an existing array element via a proxy with no set handler
// calls the defineProperty handler.

function test(arr) {
    var p = new Proxy(arr, {
        defineProperty(t, id, desc) {
            hits++;

            // ES6 draft rev 28 (2014 Oct 14) 9.1.9 step 5.e.i.
            // Since the property already exists, the system only changes
            // the value. desc is otherwise empty.
            assertEq(Object.getOwnPropertyNames(desc).join(","), "value");
            assertEq(desc.value, "ponies");
            return true;
        }
    });
    var hits = 0;
    p[0] = "ponies";
    assertEq(hits, 1);
    assertEq(arr[0], 123);
}

test([123]);
