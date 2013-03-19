function f() {
    for (var i=0; i<50; i++) {
	assertEq(customNative, undefined);
    }
}
f();
