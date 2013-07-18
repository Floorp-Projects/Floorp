function f() {
    for (var i=0; i<50; i++) {
	assertEq(customNative, undefined);
    }
}
f();

// Don't assert (bug 852798).
Object.getOwnPropertyDescriptor(this, "customNative");

// Don't assert (bug 867082).
this.__defineSetter__("customNative", unescape);
