function f(g,y,z,w) {
  return g.apply(undefined, arguments);
}

function outer() {
  var g = function() {};
  for (var i = 0; i < 20000; i++)
    f(g,2,3,4,5);
  f(g,2,3,4,5);
}

outer();
