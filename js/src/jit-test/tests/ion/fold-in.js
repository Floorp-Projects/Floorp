// Singleton
function f() {
    var res = 0;
    for (var i=0; i<500; i++)
	res += ("abcd" in Math);
    return res;
}
assertEq(f(), 0);
Math.abcd = 3;
assertEq(f(), 500);
delete Math.abcd;
assertEq(f(), 0);

// Non-singleton
function O(x) { if (x) this.x = 1; }

var arr = [];
for (var i=0; i<4; i++)
    arr.push(new O(i % 2));

function g(arr) {
    var res = 0;
    for (var i=0; i<500; i++) {
	var o = arr[i % arr.length];
	res += "x" in o;
	res += "abcd" in o;
    }
    return res;
}
assertEq(g(arr), 250);
arr[0].abcd = 3;
assertEq(g(arr), 375);
