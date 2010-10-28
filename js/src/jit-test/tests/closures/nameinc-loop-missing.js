// |jit-test| error: ReferenceError

for (var i = 0; i < 10; ++i) {
  var f = function() {
    var p = 0;
    
    function g() {
      for (var i = 0; i < 5; ++i) {
	p++;
	x++;
	print(p);
      }
    }
    
    g();
    
    print(p);
  }
  f();
}

