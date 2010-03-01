var g1 = this;
var g2 = evalcx('');

function m() {
    return x;
}
g2.m = clone(m, g2);

var x = 42;
g2.x = 99;

var a = [g1, g1, g1, g1, g2];
var b = [];

for (var i = 0; i < a.length; i++)
    b[b.length] = a[i].m();

assertEq(b[4], g2.x);

checkStats({
    recorderStarted: 1,
    recorderAborted: 0,
    traceCompleted: 1
});
