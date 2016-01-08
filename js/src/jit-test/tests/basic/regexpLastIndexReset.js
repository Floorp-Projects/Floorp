// Bug 1207922 - lastIndex should be reset to 0 when match fails.

var pattern = /abc/;
var string = 'aaaaaaaa';

function test() {
  pattern.lastIndex = 3;
  var result = pattern.exec(string);
  assertEq(result, null);
  assertEq(pattern.lastIndex, 0);
}

for (let i = 0; i < 10; i++) {
  test();
}

function test2() {
  pattern.lastIndex = 3;
  var result = pattern.test(string);
  assertEq(result, false);
  assertEq(pattern.lastIndex, 0);
}

for (let i = 0; i < 10; i++) {
  test2();
}
