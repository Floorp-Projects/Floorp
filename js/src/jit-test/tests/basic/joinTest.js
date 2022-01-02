function joinTest()
{
  var s = "";
  var a = [];
  for (var i = 0; i < 8; i++)
    a[i] = [String.fromCharCode(97 + i)];
  for (i = 0; i < 8; i++) {
    for (var j = 0; j < 8; j++)
      a[i][1 + j] = j;
  }
  for (i = 0; i < 8; i++)
    s += a[i].join(",");
  return s;
}
assertEq(joinTest(), "a,0,1,2,3,4,5,6,7b,0,1,2,3,4,5,6,7c,0,1,2,3,4,5,6,7d,0,1,2,3,4,5,6,7e,0,1,2,3,4,5,6,7f,0,1,2,3,4,5,6,7g,0,1,2,3,4,5,6,7h,0,1,2,3,4,5,6,7");
