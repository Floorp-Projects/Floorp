function bind(f) {
      return f.call.apply(f.bind, arguments);
}
function g(a, b) {}
for(var i=0; i<20; ++i) {
      g.call(undefined, {}, bind(function(){}));
}
