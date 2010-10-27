// |jit-test| error: ReferenceError

for (var i = 0; i < 10; ++i) {
  var f = function() {
    var p = 0;
    
    function g() {
      for (var i = 0; i < 5; ++i) {
	x += 5;
      }
    }
    
    g();
    
    print(p);
  }
  f();
}

