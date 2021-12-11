function g(arr) {
    var res = [];
    for (var i = 0; i < arr.length; i++) {
        var el = arr[i];
        res.push(el);
    }
    return res;
}
function f() {
    for (var i = 0; i < 2; i++) {
        var obj = {__proto__: []};
        for (var j = 0; j < 1500; j++) {
            g([13.37, obj]);
        }
    }
}
f();
