function f() {
    var x = 0;
    for(var i=0; i<5; i++) {
        (function() {
          var q = parseFloat("2");
          x += q;
        })();
    }
    return x;
}
assertEq(f(), 10);
