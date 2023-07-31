// |jit-test| skip-if: !('oomTest' in this)

// Basic Smoke Test
async function* asyncGen(n) {
    for (let i = 0; i < n; i++) {
        yield i * 2;
    }
}

function test() {
    Array.fromAsync(asyncGen(4)).then((x) => {
        assertEq(Array.isArray(x), true);
        assertEq(x.length, 4);
        assertEq(x[0], 0);
        assertEq(x[1], 2);
        assertEq(x[2], 4);
        assertEq(x[3], 6);
        done = true;
    }
    );

    drainJobQueue();
}

oomTest(test);

