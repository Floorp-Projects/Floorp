var re = /.*(a\w).*/i;
var str = "abccccccad";
for (var k = 0; k < 100; k++) {
  re.test(str);
  assertEq('ad', RegExp['$1']);
}
