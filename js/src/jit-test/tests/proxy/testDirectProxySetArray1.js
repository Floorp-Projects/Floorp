// Assigning to a missing array element (a hole) via a proxy with no set handler
// calls the defineProperty handler.

function test(id) {
    var arr = [, 1, 2, 3];
    var p = new Proxy(arr, {
        defineProperty(t, id, desc) {
            hits++;
            assertEq(desc.value, "ponies");
            assertEq(desc.enumerable, true);
            assertEq(desc.configurable, true);
            assertEq(desc.writable, true);
            return true;
        }
    });
    var hits = 0;
    p[id] = "ponies";
    assertEq(hits, 1);
    assertEq(id in arr, false);
    assertEq(arr.length, 4);
}

test(0);
test(4);
test("str");
