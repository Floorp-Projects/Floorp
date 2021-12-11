// |jit-test| --fast-warmup; --ion-offthread-compile=off

const too_big_for_float32 = 67109020;

function call_with_no_ic_data() {}

function foo() {
    call_with_no_ic_data();

    let x = too_big_for_float32;
    let result;

    // We OSR in this loop.
    for (let i = 0; i < 100; i++) {
        const float32 = Math.fround(0);

	// Create a phi with one float32-typed input
	// and one OSRValue input.
        result = float32 || x;
    }

    return result;
}

assertEq(foo(), too_big_for_float32);
