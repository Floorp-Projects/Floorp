function equalInt()
{
  var i1 = 55, one = 1, zero = 0, undef;
  var o1 = { }, o2 = { };
  var s = "5";
  var hits = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0];
  for (var i = 0; i < 5000; i++) {
    if (i1 == 55) hits[0]++;
    if (i1 != 56) hits[1]++;
    if (i1 < 56)  hits[2]++;
    if (i1 > 50)  hits[3]++;
    if (i1 <= 60) hits[4]++;
    if (i1 >= 30) hits[5]++;
    if (i1 == 7)  hits[6]++;
    if (i1 != 55) hits[7]++;
    if (i1 < 30)  hits[8]++;
    if (i1 > 90)  hits[9]++;
    if (i1 <= 40) hits[10]++;
    if (i1 >= 70) hits[11]++;
    if (o1 == o2) hits[12]++;
    if (o2 != null) hits[13]++;
    if (s < 10) hits[14]++;
    if (true < zero) hits[15]++;
    if (undef > one) hits[16]++;
    if (undef < zero) hits[17]++;
  }
  return hits.toString();
}
assertEq(equalInt(), "5000,5000,5000,5000,5000,5000,0,0,0,0,0,0,0,5000,5000,0,0,0");
