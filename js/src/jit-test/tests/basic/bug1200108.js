// |jit-test| error: 987
var obj = {length: -1, 0: 0};
Array.prototype.map.call(obj, function () {
    throw 987;
});
