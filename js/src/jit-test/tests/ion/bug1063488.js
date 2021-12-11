
function foo(a, b) {
    var x = b[0];
    for (var i = 0; i < 5; i++) {
	a[i + 1] = 0;
	x += b[i];
    }
    assertEq(x, 2);
}
function bar() {
    for (var i = 0; i < 5; i++) {
	var arr = [1,2,3,4,5,6];
	foo(arr, arr);
    }
}
bar();
