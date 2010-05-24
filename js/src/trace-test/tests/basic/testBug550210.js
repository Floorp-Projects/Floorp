function g(e) {
    return ("" + e);
}

function blah() {
    do {
        yield;
    } while ({}(p = arguments));
}
rv = blah();
try {
    for (a in rv) ;
} catch (e) {
    print("" + g(e));
}
gc();
