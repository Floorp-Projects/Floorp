function* a(x, y, z) {
    if (arguments.length !== 3) {
        throw "Wrong output";
    }
    yield x;
    yield y;
    yield z;
}
const x = a(3, 4, 5);
x.next();
