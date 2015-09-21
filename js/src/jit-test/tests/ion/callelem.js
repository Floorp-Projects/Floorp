enableNoSuchMethod();

var res = 0;
var o = {};
o.__noSuchMethod__ = function(x) { res += x; };

function f() {
    for (var i=0; i<80; i++) {
        o[i](i);
    }
}
f();
assertEq(res, 3160);
