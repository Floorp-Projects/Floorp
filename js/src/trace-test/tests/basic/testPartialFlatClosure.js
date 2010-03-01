assertEq((('-r', function (s) {
    function C(i) {
        this.m = function () { return i * t; }
    }
    var t = s;
    var a = [];
    for (var i = 0; i < 5; i++)
        a[a.length] = new C(i);
    return a;
})(42))[4].m(), 168);

checkStats({
    recorderStarted: 1,
    recorderAborted: 0,
    traceCompleted: 1,
});
