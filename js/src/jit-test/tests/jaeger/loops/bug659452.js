test();
function test() {
    var t = function () { return function printStatus() {}; };
    for (var j = 0; j < 10; j++) t["-1"]
}
