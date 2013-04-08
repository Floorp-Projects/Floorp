function f() {
    var s='';
    for (var i=0; i < 5000; i++)
	s += 'x' + i + '=' + i + ';\n';
    return s;
}
eval(f());
