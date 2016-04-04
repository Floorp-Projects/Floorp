// Binary: cache/js-dbg-64-daefd30072a6-linux
// Flags:
//
function f(param) {
  var w;
  return eval("\
    (function(){\
      this.__defineGetter__(\"y\", function()({\
        x: function(){ return w }()\
      }))\
    });\
  ");
}
(f())();
(new Function("eval(\"y\")"))();
