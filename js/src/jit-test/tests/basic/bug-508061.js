function loop() {
    var x;
    for (var i = 0; i < 9; i++)
        x = {1.5: 1};
    return x;
}

loop(); // record
Object.prototype.__defineSetter__('1.5', function () { return 'BAD'; });
var x = loop(); // playback
assertEq(x["1.5"], 1);