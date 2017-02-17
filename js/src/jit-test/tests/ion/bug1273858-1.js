// |jit-test| --no-threads

function t1() {
    let x = [];

    for (let k = 0; k < 100; ++k)
        x[k] = () => k; // Lexical capture

    try {
        eval("k");
        throw false;
    }
    catch (e) {
        if (!(e instanceof ReferenceError))
            throw "Loop index escaped block";
    }

    for (var i = 0; i < 100; ++i)
        if (x[i]() != i)
            throw "Bad let capture";
}
t1();
t1();
t1();
t1();

function t2() {
    let x = [];
    let y = {};

    for (var i = 0; i < 100; ++i)
        x[i] = i;

    for (const k of x)
        y[k] = () => k; // Lexical capture

    try {
        eval("k");
        throw false;
    }
    catch (e) {
        if (!(e instanceof ReferenceError))
            throw "Loop index escaped block";
    }

    for (var i = 0; i < 100; ++i)
        if (y[i]() != i)
            throw "Bad const capture";
}
t2();
t2();
t2();
t2();
