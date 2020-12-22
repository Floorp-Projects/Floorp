function f() {
    var o = [1, 2, 3];
    o.constructor = function() {
      return new Uint8Array(3);
    };
    for (var i=0; i<10; i++)
        o.filter(x => true);
}
f();
