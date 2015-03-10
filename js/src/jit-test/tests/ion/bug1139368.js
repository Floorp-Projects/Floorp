// |jit-test| error: TypeError
function f() {
    for (var i=2; i<2; i++) {
	var a = /a/;
    }
    for (var i=0; i<2; i++) {
	a.exec("aaa");
    }
}
f();
