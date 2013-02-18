// Don't assert in the decompiler.
function f() {
    var o = null;

    try {
	delete o.prop;
    } catch(e) {}

    try {
	delete o[1];
    } catch(e) {}

    try {
	o[{}]++;
    } catch(e) {}
}
f();
