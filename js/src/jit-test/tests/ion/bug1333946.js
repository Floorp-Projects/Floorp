// |jit-test| exitstatus: 6;

for (var x of [0]) {
    timeout(0.001);
    for (;;) {}
}
