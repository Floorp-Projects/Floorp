function f() {
  var o;
  for (var i=0; i < 1000; i++) {
    o = new Object();
  }
}

var d = new Date();
f();
print("took", (new Date()) - d, "ms.");
    
