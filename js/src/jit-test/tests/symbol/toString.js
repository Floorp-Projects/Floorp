// ToString(symbol) throws a TypeError.

var obj;
for (var i = 0; i < 10; i++) {
    try {
        obj = new String(Symbol());
    } catch (exc) {}
}
