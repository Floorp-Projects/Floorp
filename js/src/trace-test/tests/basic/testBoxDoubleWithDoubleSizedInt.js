function testBoxDoubleWithDoubleSizedInt()
{
  var i = 0;
  var a = new Array(3);

  while (i < a.length)
    a[i++] = 0x5a827999;
  return a.join(",");
}
assertEq(testBoxDoubleWithDoubleSizedInt(), "1518500249,1518500249,1518500249");
