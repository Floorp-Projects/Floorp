// Make sure that we can find own, enumerable symbols.
var symbol = Symbol("bad");
var symbol2 = Symbol("good");
var proxy = new Proxy({}, {
    ownKeys() {
        return [symbol, symbol2];
    },
    getOwnPropertyDescriptor(target, name) {
        if (name == symbol)
            return {configurable: true, enumerable: false, value: {}};
        // Only this enumerable symbol should be defined.
        if (name == symbol2)
            return {configurable: true, enumerable: true, value: {}};
        assertEq(true, false);
    },
    get(target, name) {
        // Slightly confusing, but these are the descriptors that defineProperties
        // is going to define on the object.
        if (name == symbol)
            return {configurable: true, value: "bad"};
        if (name == symbol2)
            return {configurable: true, value: "good"};
        assertEq(true, false);
    }
});
assertEq(Object.getOwnPropertySymbols(proxy).length, 2);

var obj = {};
Object.defineProperties(obj, proxy);
assertEq(Object.getOwnPropertySymbols(obj).length, 1);
assertEq(symbol in obj, false);
assertEq(symbol2 in obj, true);
assertEq(obj[symbol2], "good");
