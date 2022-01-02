// |jit-test| error: InternalError: too much recursion

function f() {
    var phi1 = 0;
    var phi2 = 0;
    while (true) {
        if (!phi2) {
            var add = phi1 + 1;
            f(add);
            if (!phi2)
                return;
            phi1 = 1;
            phi2 = 0;
        }
    }
}

f(0);
