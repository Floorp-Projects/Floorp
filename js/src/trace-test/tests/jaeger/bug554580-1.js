// |trace-test| error: TypeError
for (var a = 0; a < 7; ++a) {
    if (a == 1) {
        Iterator()
    }
}

