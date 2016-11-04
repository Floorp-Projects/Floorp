let tenured = {};
gc();
for (let i = 0; i < 100000; i++) {
    tenured[i/2] = {};
}
