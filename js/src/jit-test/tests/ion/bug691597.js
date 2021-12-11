// |jit-test| error: TypeError
function f() {
	-null();
}
f();
