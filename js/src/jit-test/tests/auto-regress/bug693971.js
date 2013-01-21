// Binary: cache/js-dbg-64-07c668448519-linux
// Flags: -m -n
//

function testContinue() {
    for (i = 0; i < 100; ++i) {
        if ([1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15])
            continue;
    }
    return 0;
}
assertEq(testContinue(), 0);
