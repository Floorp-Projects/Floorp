let proxy = new Proxy({
    a: 1,
    b: 2,
    c: 3
}, {
    enumerate() {
        // Should not be invoked.
        assertEq(false, true);
    },

    ownKeys() {
        return ['a', 'b'];
    }
});

let object = Object.create(proxy);
object.d = 4;

assertEq([x for (x in object)].toString(), "d,a,b");
