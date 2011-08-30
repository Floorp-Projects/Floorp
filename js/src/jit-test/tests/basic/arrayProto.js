
for (var i = 0; i < 15; i++) {
  var x = Object.create([]);
  assertEq(x.length, 0);
}

for (var i = 0; i < 15; i++) {
  function foo() {}
  foo.prototype = [];
  var x = new foo();
  assertEq(x.length, 0);
}
