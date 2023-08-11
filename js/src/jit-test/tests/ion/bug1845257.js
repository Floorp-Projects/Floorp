// |jit-test| --no-threads; --baseline-warmup-threshold=10; --ion-warmup-threshold=100; --fuzzing-safe; --gc-zeal=10

const v7 = {};

function foo(n) {
    if (n == 0) return;
    const v13 = Object(Object);
    const v14 = v13(v7);
    const v15 = v13.create(v13);
    v15.setPrototypeOf(v14, v13);
    const v18 = v15.assign(v14).create(v14); // 1
    v18.is(v18, v18);                        // 2
    foo(n-1);
}
foo(2000);
