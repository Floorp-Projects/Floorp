const C = function (a, b, c) {
    return function C() {
        this.m1 = function () a;
        this.m2 = function () b;
        this.m3 = function () c;
    }
}(2,3,4);
var c = new C();
var d = function (e) {return {m0: function () e}}(5);
for (var i = 0; i < 5; i++)
    d.m0();
C.call(d);
d.__iterator__ = function() {yield 55};
for (i = 0; i < 5; i++) {
    for (j in d)
        print(j);
}
