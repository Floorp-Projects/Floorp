// |jit-test| --ion-eager; --no-threads; --arm-sim-icache-checks; --gc-zeal=14
Object.getOwnPropertyNames(this);
for (var i = 0; i < 1; ++i) {
    [Array];
    [ArrayBuffer];
}
