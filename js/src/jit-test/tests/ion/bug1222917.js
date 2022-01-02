function f() {
    var x = [];
    for (var i=0; i<10; i++)
	x.length = x;
}
f();
