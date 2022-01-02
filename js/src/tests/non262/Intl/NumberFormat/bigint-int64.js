// |reftest| skip-if(!this.hasOwnProperty("Intl"))

// Ensure the int64_t optimization when formatting a BigInt value works correctly by testing with
// various integers around the (u)int[32,64] limits.

const limits = {
    int32: {
        min: -0x80000000n,
        max:  0x7FFFFFFFn,
    },
    uint32: {
        min: 0n,
        max: 0xFFFFFFFFn
    },
    int64: {
        min: -0x8000000000000000n,
        max:  0x7FFFFFFFFFFFFFFFn,
    },
    uint64: {
        min: 0n,
        max: 0xFFFFFFFFFFFFFFFFn
    },
};

const nf = new Intl.NumberFormat("en", {useGrouping: false});

const diff = 10n;

for (const int of Object.values(limits)) {
    for (let i = -diff; i <= diff; ++i) {
        let n = int.min + i;
        assertEq(nf.format(n), n.toString());

        let m = int.max + i;
        assertEq(nf.format(m), m.toString());
    }
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
