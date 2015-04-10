// |jit-test| error: closing generator
var finally3;
function gen() {
    try {
        try {
            yield 1;
        } finally {
            finally3();
        }
    } catch (e) {
        yield finally3 === parseInt;
    }
}
iter = gen();
iter.next();
iter.close();
