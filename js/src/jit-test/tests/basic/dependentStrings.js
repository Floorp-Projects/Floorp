function dependentStrings()
{
  var a = [];
  var t = "abcdefghijklmnopqrst";
  for (var i = 0; i < 10; i++) {
    var s = t.substring(2*i, 2*i + 2);
    a[i] = s + s.length;
  }
  return a.join("");
}
assertEq(dependentStrings(), "ab2cd2ef2gh2ij2kl2mn2op2qr2st2");
