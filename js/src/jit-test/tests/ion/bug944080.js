
function intLength (a, l) {
    var res = 0;
    for (var i = 0; i < l; i++)
	res += a.length;
}
intLength([0,1,2,3,4,5,6,7,8,9], 10)
intLength(new Uint8Array(10), 10)
function test() {
    var a = "abc".split("");
    var res = 0;
    for (var i=0; i<20; i++)
	res += a.length;
    return res;
}
Object.prototype.length = function(){};
assertEq(test(), 60);
