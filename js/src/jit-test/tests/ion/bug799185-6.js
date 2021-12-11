function f(a, b, c) {
    while (a) {
        let x;
        if (b) {
            if (c) {
                d();
                break;  // hidden LEAVEBLOCK, then GOTO
            }
            break; // another hidden LEAVEBLOCK, then GOTO
        }
    }
    null.x;
}

try {
    f();
} catch (x) {
    ;
}
