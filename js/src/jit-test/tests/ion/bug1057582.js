Object.defineProperty(Object.prototype, "0", {configurable: true});
delete Object.prototype[0];

function makeArray() {
    var arr = [];
    for (var i = 0; i < 10; i++)
        arr.push(true);
    return arr;
}
makeArray();
