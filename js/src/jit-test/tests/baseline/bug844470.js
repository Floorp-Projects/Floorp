// |jit-test| --blinterp-eager
// See bug 1702259 for rationale for the above jit-test line.
function f() {
    var s='';
    for (var i=0; i < 20000; i++)
	s += 'x' + i + '=' + i + ';\n';
    return s;
}
eval(f());
