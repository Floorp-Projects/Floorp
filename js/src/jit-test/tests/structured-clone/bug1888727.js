function test() {
    // Construct a structured clone of a random BigInt value.
    const n = 0xfeeddeadbeef2dadfeeddeadbeef2dadfeeddeadbeef2dadfeeddeadbeef2dadn;
    const s = serialize(n, [], {scope: 'DifferentProcess'});
    assertEq(deserialize(s), n);

    // Truncate it by chopping off the last 8 bytes.
    s.clonebuffer = s.arraybuffer.slice(0, -8);

    // Deserialization should now throw a catchable exception.
    try {
        deserialize(s);
        // The bug was throwing an uncatchable error, so this next assertion won't
        // be reached in either the buggy or fixed code.
        assertEq(true, false, "should have thrown truncation error");
    } catch (e) {
        assertEq(e.message.includes("truncated"), true);
    }
}

test();
