var o = {};
var arr = [o,o,o,o,o,o,o,o,o,o,o,o,o];
var out = [];

const OUTER = 10;

for (var i = 0; i < 10; ++i) {
    for (var j = 0; j < arr.length; ++j) {
        out.push(String.prototype.indexOf.call(arr[i], 'object'));
    }
}

assertEq(out.length, 10 * arr.length);
for (var i = 0; i < out.length; ++i)
    assertEq(out[i], 1);

checkStats({
    traceCompleted:2,
    recorderAborted:1
});
