var arr = [];
for (var i=0; i<20; i++) {
    arr.push(new Int32Array(2000));
}
arr.push([null, null]);

function test(o, x) {
    assertEq(o[0], x);
}

function f() {
    for (var i=0; i<3100; i++) {
	var o = arr[i % arr.length];
	if (o.length > 10 || i > 2000) {
	    var val = (i > 3000 ? 1 : null);
	    o[0] = val;
	    if (o.length < 5)
		test(o, val);
	}
    }
}
f();
