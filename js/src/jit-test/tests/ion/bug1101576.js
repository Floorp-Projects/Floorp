// Random chosen test: js/src/jit-test/tests/ion/bug928423.js
o = {
        a: 1,
            b: 1
}
print(1);
for (var x = 0; x < 2; x++) {
    print(2);
    o["a1".substr(0, 1)]
    o["b1".substr(0, 1)]
}
print(3);
// jsfunfuzz
"a" + "b"
