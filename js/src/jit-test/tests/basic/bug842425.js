// |jit-test| error: TypeError

function g() {
  var a = [];
  for (var i = 0; i < 10; i++)
    a.push(i, 1.5);
  for (var i = 0; i < 32 ; i++) {
    print(i);
    a[i].m = function() {} 
  }
}
g();
