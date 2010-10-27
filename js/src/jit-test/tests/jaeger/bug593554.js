/* Don't assert. */
var b = 7;
var a = [];
for (var j = 0; j < 7; ++j) {
    var d = {};
    a.push(b >> d);
}
assertEq(a.toString(), '7,7,7,7,7,7,7');
