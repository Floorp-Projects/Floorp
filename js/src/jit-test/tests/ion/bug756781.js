function AddTestCase( description, expect, actual ) {
  new TestCase( SECTION, description, expect, actual );
}
function TestCase(n, d, e, a) {}
var SECTION = "String/match-004.js";
re = /0./;
s = 10203040506070809000;
Number.prototype.match = String.prototype.match;
AddRegExpCases(  re, "re = " + re , s, String(s), 1, ["02"]);
AddRegExpCases(  re, re, s, ["02"]);
function AddRegExpCases(
  regexp, str_regexp, string, str_string, index, matches_array ) {
  if ( regexp.exec(string) == null || matches_array == null ) {
    AddTestCase( string.match(regexp) );
  }
  AddTestCase( string.match(regexp).input );
  gczeal(4);
}

