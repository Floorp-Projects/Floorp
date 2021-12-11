// |jit-test| error:InternalError

// Binary: cache/js-dbg-32-756dd46daf6c-linux
// Flags: -j
//
var d = {
  p: function () {
         for (var i = 0; i < 9; ++i);
         with (d) { q(); }
     }
};
d.q = function() { eval('this.p()'); }
d.p();
