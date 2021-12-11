function t() {
    var iter = 0;
    function a(w) {
	var a = new Array(w);
	assertEq(a.length, w);
    }
    function r() {
	a(3);
	a(4);
    }
    for (var i=0; i<100; i++) {
	r();
    }
}
t();
