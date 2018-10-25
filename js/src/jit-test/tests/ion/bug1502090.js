function f(o) {
   var a = [o];
   a.length = a[0];
   var useless = function() {}
   var sz = Array.prototype.push.call(a, 42, 43);
   (function(){
       sz;
   })(new Boolean(false));
}
for (var i = 0; i < 2; i++) {
   f(1);
}
f(2);
