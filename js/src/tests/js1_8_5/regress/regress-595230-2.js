// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/
// Contributors: Jason Orendorff <jorendorff@mozilla.com>

var s = evalcx("");
delete s.Object;
evalcx("var x;", s);

this.reportCompare(0, 0, "ok");
