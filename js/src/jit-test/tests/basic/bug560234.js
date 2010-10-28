function test() {
    var a = ['x', '', '', '', '', '', '', 'x'];
    var b = '';
    for (var i = 0; i < a.length; i++) {
        (function() {
            a[i].replace(/x/, function() { return b; });
        }());
    }
}
test();  // should NOT get a ReferenceError for b on trace
