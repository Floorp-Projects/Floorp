// Binary: cache/js-dbg-64-94ceb173baed-linux
// Flags: -m -n -a
//

gczeal(2);
function iso(d) {
  return new Date(d).toISOString();
}
function check(s, millis){
  description = "Date.parse('"+s+"') == '"+iso(millis)+"'";
}
function dd(year, month, day, hour, minute, second, millis){
  return Date.UTC(year, month-1, day, hour, minute, second, millis);
}
function TZAtDate(d){
  return d.getTimezoneOffset() * 60000;
}
function TZInMonth(month){
  return TZAtDate(new Date(dd(2009,month,1,0,0,0,0)));
}
JulTZ = TZInMonth(7);
check("2009-07-23T19:53:21.001+12:00", dd(2009,7,23,7,53,21,1));
check("2009-07-23T19:53:21+12:00",     dd(2009,7,23,7,53,21,0));
check("2009-07-23T19:53+12:00",        dd(2009,7,23,7,53,0,0));
check("2009-07T19:53:21.001",          dd(2009,0Xe ,1,19,53,21,1)+JulTZ);
check("2009-07T19:53:21",              dd(2009,7,1,19,53,21,0)+JulTZ);
check("2009-07T19:53",                 dd(2009,7,1,19,53,0,0)+JulTZ);
check("2009-07-23T19:53:21.001+12:00", dd(2009,7,23,7,53,21,1));
check("2009-07-23T00:00:00.000-07:00", dd(2009,7,23,7,0,0,0));
check("2009-07-23T24:00:00.000-07:00", dd(2009,7,24,7,0,0,0));
