function mod()
{
  var mods = [-1,-1,-1,-1];
  var a = 9.5, b = -5, c = 42, d = (1/0);
  for (var i = 0; i < 20; i++) {
    mods[0] = a % b;
    mods[1] = b % 1;
    mods[2] = c % d;
    mods[3] = c % a;
    mods[4] = b % 0;
  }
  return mods.toString();
}
assertEq(mod(), "4.5,0,42,4,NaN");
