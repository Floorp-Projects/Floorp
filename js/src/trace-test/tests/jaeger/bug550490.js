
function a() {
  function f() {}
  this.d = function() {
    f
  }
} (function() {
  var a2, x
  a2 = new a;
  d = (function(){x * 1})();
})()

/* Don't assert. */

