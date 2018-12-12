print("Stress test of ropes");

function createRopes() {
    const ropes = {};

    let j = 88;
    function advance() {
        // This is totally made up and probably stupid.
        j = ((((j * 7) >> 2) + j) + (j << 3)) | 0;
        return j;
    }

    function randomBalancedRope(height) {
        if (height == 0)
            return "s" + j;

        const left = randomBalancedRope(height - 1);
        advance();
        const right = randomBalancedRope(height - 1);
        advance();
        return newRope(left, right, {nursery:(j & 1)});
    }

    // Construct a fairly big random rope first. If we did it later, then the
    // chances of it all ending up tenured are higher.
    ropes.balanced = randomBalancedRope(10);

    ropes.simple = newRope("a", "b");
    ropes.simple_tenured = newRope("a", "b", {nursery:false});
    ropes.tenured_nursery = newRope("a", newRope("b", "c", {nursery:true}), {nursery:false});
    ropes.nursery_tenured = newRope("a", newRope("b", "c", {nursery:false}), {nursery:true});

    return ropes;
}

const ropes = createRopes();

// Flatten them all.
for (const [name, rope] of Object.entries(ropes))
    ensureFlatString(rope);

// GC with them all live.
let ropes2 = createRopes();
gc();

// GC with them all dead.
ropes2 = null;
gc();

if (typeof reportCompare === "function")
    reportCompare(true, true);
