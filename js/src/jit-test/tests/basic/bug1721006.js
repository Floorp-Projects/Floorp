// |jit-test| --no-threads; --baseline-warmup-threshold=10; --ion-warmup-threshold=100

function foo() {
    let x = 1;
    let y = 1;
    let z = 1;
    for (let i = 0; i < 4; i++) {}
    for (let i = 0; i < 1000; i++) {
        y &= x;
        for (let j = 0; j < 100; j++) {}
        x = z;
        for (let j = 0; j < 8; j++) {}
    }
}
foo();
