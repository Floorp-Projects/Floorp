load(libdir + 'asserts.js');

for (var code of ["08", "09", "01238", "01239", "08e+1"]) {
    assertThrowsInstanceOf(() => Function(code), SyntaxError);
    assertThrowsInstanceOf(() => eval(code), SyntaxError);
}

var ok = [0.8, 0.08, 0e8, 1e08, 1e+08];
