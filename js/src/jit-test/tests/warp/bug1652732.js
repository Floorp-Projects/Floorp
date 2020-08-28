function test() {
  var obj = {};
  obj[{}] = 1;
  f = () => { for (var x of obj) {} };
}
for (var i = 0; i < 5; i++) {
  test();
}
