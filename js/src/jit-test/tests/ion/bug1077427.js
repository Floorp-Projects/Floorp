
(function f() {
  var i;
  var x = 3;
  var o1 = { a: x };
  var o2 = { a: 2 };
  for (i = 0; i < 5; i++)
    o2.a = x;
  Object.preventExtensions({ a: 1 });
  for (i = 0; i < 5; i++)
    ;
})();
