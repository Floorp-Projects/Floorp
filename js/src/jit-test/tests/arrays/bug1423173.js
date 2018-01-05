// |jit-test| --baseline-eager
Array.prototype.push(1);
Object.freeze([].__proto__);
var x = [];
var c = 0;
for (var j = 0; j < 5; ++j) {
    try {
        x.push(function() {});
    } catch (e) {
        c++;
    }
}
assertEq(c, j);
