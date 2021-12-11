// |jit-test| error: TypeError

function f() {    }
function g() {    }
var x = [f,f,f,undefined,g];
for (var i = 0; i < 5; ++i)
  y = x[i]("x");
