// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

var gTestfile = 'stringify-special-escapes.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 512266;
var summary =
  "JSON.stringify of \\b\\f\\n\\r\\t should use one-character escapes, not hex";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

assertEq(JSON.stringify("\u0000"), '"\\u0000"');
assertEq(JSON.stringify("\u0001"), '"\\u0001"');
assertEq(JSON.stringify("\u0002"), '"\\u0002"');
assertEq(JSON.stringify("\u0003"), '"\\u0003"');
assertEq(JSON.stringify("\u0004"), '"\\u0004"');
assertEq(JSON.stringify("\u0005"), '"\\u0005"');
assertEq(JSON.stringify("\u0006"), '"\\u0006"');
assertEq(JSON.stringify("\u0007"), '"\\u0007"');
assertEq(JSON.stringify("\u0008"), '"\\b"');
assertEq(JSON.stringify("\u0009"), '"\\t"');
assertEq(JSON.stringify("\u000A"), '"\\n"');
assertEq(JSON.stringify("\u000B"), '"\\u000b"');
assertEq(JSON.stringify("\u000C"), '"\\f"');
assertEq(JSON.stringify("\u000D"), '"\\r"');
assertEq(JSON.stringify("\u000E"), '"\\u000e"');
assertEq(JSON.stringify("\u000F"), '"\\u000f"');
assertEq(JSON.stringify("\u0010"), '"\\u0010"');
assertEq(JSON.stringify("\u0011"), '"\\u0011"');
assertEq(JSON.stringify("\u0012"), '"\\u0012"');
assertEq(JSON.stringify("\u0013"), '"\\u0013"');
assertEq(JSON.stringify("\u0014"), '"\\u0014"');
assertEq(JSON.stringify("\u0015"), '"\\u0015"');
assertEq(JSON.stringify("\u0016"), '"\\u0016"');
assertEq(JSON.stringify("\u0017"), '"\\u0017"');
assertEq(JSON.stringify("\u0018"), '"\\u0018"');
assertEq(JSON.stringify("\u0019"), '"\\u0019"');
assertEq(JSON.stringify("\u001A"), '"\\u001a"');
assertEq(JSON.stringify("\u001B"), '"\\u001b"');
assertEq(JSON.stringify("\u001C"), '"\\u001c"');
assertEq(JSON.stringify("\u001D"), '"\\u001d"');
assertEq(JSON.stringify("\u001E"), '"\\u001e"');
assertEq(JSON.stringify("\u001F"), '"\\u001f"');
assertEq(JSON.stringify("\u0020"), '" "');

assertEq(JSON.stringify("\\u0000"), '"\\\\u0000"');
assertEq(JSON.stringify("\\u0001"), '"\\\\u0001"');
assertEq(JSON.stringify("\\u0002"), '"\\\\u0002"');
assertEq(JSON.stringify("\\u0003"), '"\\\\u0003"');
assertEq(JSON.stringify("\\u0004"), '"\\\\u0004"');
assertEq(JSON.stringify("\\u0005"), '"\\\\u0005"');
assertEq(JSON.stringify("\\u0006"), '"\\\\u0006"');
assertEq(JSON.stringify("\\u0007"), '"\\\\u0007"');
assertEq(JSON.stringify("\\u0008"), '"\\\\u0008"');
assertEq(JSON.stringify("\\u0009"), '"\\\\u0009"');
assertEq(JSON.stringify("\\u000A"), '"\\\\u000A"');
assertEq(JSON.stringify("\\u000B"), '"\\\\u000B"');
assertEq(JSON.stringify("\\u000C"), '"\\\\u000C"');
assertEq(JSON.stringify("\\u000D"), '"\\\\u000D"');
assertEq(JSON.stringify("\\u000E"), '"\\\\u000E"');
assertEq(JSON.stringify("\\u000F"), '"\\\\u000F"');
assertEq(JSON.stringify("\\u0010"), '"\\\\u0010"');
assertEq(JSON.stringify("\\u0011"), '"\\\\u0011"');
assertEq(JSON.stringify("\\u0012"), '"\\\\u0012"');
assertEq(JSON.stringify("\\u0013"), '"\\\\u0013"');
assertEq(JSON.stringify("\\u0014"), '"\\\\u0014"');
assertEq(JSON.stringify("\\u0015"), '"\\\\u0015"');
assertEq(JSON.stringify("\\u0016"), '"\\\\u0016"');
assertEq(JSON.stringify("\\u0017"), '"\\\\u0017"');
assertEq(JSON.stringify("\\u0018"), '"\\\\u0018"');
assertEq(JSON.stringify("\\u0019"), '"\\\\u0019"');
assertEq(JSON.stringify("\\u001A"), '"\\\\u001A"');
assertEq(JSON.stringify("\\u001B"), '"\\\\u001B"');
assertEq(JSON.stringify("\\u001C"), '"\\\\u001C"');
assertEq(JSON.stringify("\\u001D"), '"\\\\u001D"');
assertEq(JSON.stringify("\\u001E"), '"\\\\u001E"');
assertEq(JSON.stringify("\\u001F"), '"\\\\u001F"');
assertEq(JSON.stringify("\\u0020"), '"\\\\u0020"');


assertEq(JSON.stringify("a\u0000"), '"a\\u0000"');
assertEq(JSON.stringify("a\u0001"), '"a\\u0001"');
assertEq(JSON.stringify("a\u0002"), '"a\\u0002"');
assertEq(JSON.stringify("a\u0003"), '"a\\u0003"');
assertEq(JSON.stringify("a\u0004"), '"a\\u0004"');
assertEq(JSON.stringify("a\u0005"), '"a\\u0005"');
assertEq(JSON.stringify("a\u0006"), '"a\\u0006"');
assertEq(JSON.stringify("a\u0007"), '"a\\u0007"');
assertEq(JSON.stringify("a\u0008"), '"a\\b"');
assertEq(JSON.stringify("a\u0009"), '"a\\t"');
assertEq(JSON.stringify("a\u000A"), '"a\\n"');
assertEq(JSON.stringify("a\u000B"), '"a\\u000b"');
assertEq(JSON.stringify("a\u000C"), '"a\\f"');
assertEq(JSON.stringify("a\u000D"), '"a\\r"');
assertEq(JSON.stringify("a\u000E"), '"a\\u000e"');
assertEq(JSON.stringify("a\u000F"), '"a\\u000f"');
assertEq(JSON.stringify("a\u0010"), '"a\\u0010"');
assertEq(JSON.stringify("a\u0011"), '"a\\u0011"');
assertEq(JSON.stringify("a\u0012"), '"a\\u0012"');
assertEq(JSON.stringify("a\u0013"), '"a\\u0013"');
assertEq(JSON.stringify("a\u0014"), '"a\\u0014"');
assertEq(JSON.stringify("a\u0015"), '"a\\u0015"');
assertEq(JSON.stringify("a\u0016"), '"a\\u0016"');
assertEq(JSON.stringify("a\u0017"), '"a\\u0017"');
assertEq(JSON.stringify("a\u0018"), '"a\\u0018"');
assertEq(JSON.stringify("a\u0019"), '"a\\u0019"');
assertEq(JSON.stringify("a\u001A"), '"a\\u001a"');
assertEq(JSON.stringify("a\u001B"), '"a\\u001b"');
assertEq(JSON.stringify("a\u001C"), '"a\\u001c"');
assertEq(JSON.stringify("a\u001D"), '"a\\u001d"');
assertEq(JSON.stringify("a\u001E"), '"a\\u001e"');
assertEq(JSON.stringify("a\u001F"), '"a\\u001f"');
assertEq(JSON.stringify("a\u0020"), '"a "');

assertEq(JSON.stringify("a\\u0000"), '"a\\\\u0000"');
assertEq(JSON.stringify("a\\u0001"), '"a\\\\u0001"');
assertEq(JSON.stringify("a\\u0002"), '"a\\\\u0002"');
assertEq(JSON.stringify("a\\u0003"), '"a\\\\u0003"');
assertEq(JSON.stringify("a\\u0004"), '"a\\\\u0004"');
assertEq(JSON.stringify("a\\u0005"), '"a\\\\u0005"');
assertEq(JSON.stringify("a\\u0006"), '"a\\\\u0006"');
assertEq(JSON.stringify("a\\u0007"), '"a\\\\u0007"');
assertEq(JSON.stringify("a\\u0008"), '"a\\\\u0008"');
assertEq(JSON.stringify("a\\u0009"), '"a\\\\u0009"');
assertEq(JSON.stringify("a\\u000A"), '"a\\\\u000A"');
assertEq(JSON.stringify("a\\u000B"), '"a\\\\u000B"');
assertEq(JSON.stringify("a\\u000C"), '"a\\\\u000C"');
assertEq(JSON.stringify("a\\u000D"), '"a\\\\u000D"');
assertEq(JSON.stringify("a\\u000E"), '"a\\\\u000E"');
assertEq(JSON.stringify("a\\u000F"), '"a\\\\u000F"');
assertEq(JSON.stringify("a\\u0010"), '"a\\\\u0010"');
assertEq(JSON.stringify("a\\u0011"), '"a\\\\u0011"');
assertEq(JSON.stringify("a\\u0012"), '"a\\\\u0012"');
assertEq(JSON.stringify("a\\u0013"), '"a\\\\u0013"');
assertEq(JSON.stringify("a\\u0014"), '"a\\\\u0014"');
assertEq(JSON.stringify("a\\u0015"), '"a\\\\u0015"');
assertEq(JSON.stringify("a\\u0016"), '"a\\\\u0016"');
assertEq(JSON.stringify("a\\u0017"), '"a\\\\u0017"');
assertEq(JSON.stringify("a\\u0018"), '"a\\\\u0018"');
assertEq(JSON.stringify("a\\u0019"), '"a\\\\u0019"');
assertEq(JSON.stringify("a\\u001A"), '"a\\\\u001A"');
assertEq(JSON.stringify("a\\u001B"), '"a\\\\u001B"');
assertEq(JSON.stringify("a\\u001C"), '"a\\\\u001C"');
assertEq(JSON.stringify("a\\u001D"), '"a\\\\u001D"');
assertEq(JSON.stringify("a\\u001E"), '"a\\\\u001E"');
assertEq(JSON.stringify("a\\u001F"), '"a\\\\u001F"');
assertEq(JSON.stringify("a\\u0020"), '"a\\\\u0020"');


assertEq(JSON.stringify("\u0000Q"), '"\\u0000Q"');
assertEq(JSON.stringify("\u0001Q"), '"\\u0001Q"');
assertEq(JSON.stringify("\u0002Q"), '"\\u0002Q"');
assertEq(JSON.stringify("\u0003Q"), '"\\u0003Q"');
assertEq(JSON.stringify("\u0004Q"), '"\\u0004Q"');
assertEq(JSON.stringify("\u0005Q"), '"\\u0005Q"');
assertEq(JSON.stringify("\u0006Q"), '"\\u0006Q"');
assertEq(JSON.stringify("\u0007Q"), '"\\u0007Q"');
assertEq(JSON.stringify("\u0008Q"), '"\\bQ"');
assertEq(JSON.stringify("\u0009Q"), '"\\tQ"');
assertEq(JSON.stringify("\u000AQ"), '"\\nQ"');
assertEq(JSON.stringify("\u000BQ"), '"\\u000bQ"');
assertEq(JSON.stringify("\u000CQ"), '"\\fQ"');
assertEq(JSON.stringify("\u000DQ"), '"\\rQ"');
assertEq(JSON.stringify("\u000EQ"), '"\\u000eQ"');
assertEq(JSON.stringify("\u000FQ"), '"\\u000fQ"');
assertEq(JSON.stringify("\u0010Q"), '"\\u0010Q"');
assertEq(JSON.stringify("\u0011Q"), '"\\u0011Q"');
assertEq(JSON.stringify("\u0012Q"), '"\\u0012Q"');
assertEq(JSON.stringify("\u0013Q"), '"\\u0013Q"');
assertEq(JSON.stringify("\u0014Q"), '"\\u0014Q"');
assertEq(JSON.stringify("\u0015Q"), '"\\u0015Q"');
assertEq(JSON.stringify("\u0016Q"), '"\\u0016Q"');
assertEq(JSON.stringify("\u0017Q"), '"\\u0017Q"');
assertEq(JSON.stringify("\u0018Q"), '"\\u0018Q"');
assertEq(JSON.stringify("\u0019Q"), '"\\u0019Q"');
assertEq(JSON.stringify("\u001AQ"), '"\\u001aQ"');
assertEq(JSON.stringify("\u001BQ"), '"\\u001bQ"');
assertEq(JSON.stringify("\u001CQ"), '"\\u001cQ"');
assertEq(JSON.stringify("\u001DQ"), '"\\u001dQ"');
assertEq(JSON.stringify("\u001EQ"), '"\\u001eQ"');
assertEq(JSON.stringify("\u001FQ"), '"\\u001fQ"');
assertEq(JSON.stringify("\u0020Q"), '" Q"');

assertEq(JSON.stringify("\\u0000Q"), '"\\\\u0000Q"');
assertEq(JSON.stringify("\\u0001Q"), '"\\\\u0001Q"');
assertEq(JSON.stringify("\\u0002Q"), '"\\\\u0002Q"');
assertEq(JSON.stringify("\\u0003Q"), '"\\\\u0003Q"');
assertEq(JSON.stringify("\\u0004Q"), '"\\\\u0004Q"');
assertEq(JSON.stringify("\\u0005Q"), '"\\\\u0005Q"');
assertEq(JSON.stringify("\\u0006Q"), '"\\\\u0006Q"');
assertEq(JSON.stringify("\\u0007Q"), '"\\\\u0007Q"');
assertEq(JSON.stringify("\\u0008Q"), '"\\\\u0008Q"');
assertEq(JSON.stringify("\\u0009Q"), '"\\\\u0009Q"');
assertEq(JSON.stringify("\\u000AQ"), '"\\\\u000AQ"');
assertEq(JSON.stringify("\\u000BQ"), '"\\\\u000BQ"');
assertEq(JSON.stringify("\\u000CQ"), '"\\\\u000CQ"');
assertEq(JSON.stringify("\\u000DQ"), '"\\\\u000DQ"');
assertEq(JSON.stringify("\\u000EQ"), '"\\\\u000EQ"');
assertEq(JSON.stringify("\\u000FQ"), '"\\\\u000FQ"');
assertEq(JSON.stringify("\\u0010Q"), '"\\\\u0010Q"');
assertEq(JSON.stringify("\\u0011Q"), '"\\\\u0011Q"');
assertEq(JSON.stringify("\\u0012Q"), '"\\\\u0012Q"');
assertEq(JSON.stringify("\\u0013Q"), '"\\\\u0013Q"');
assertEq(JSON.stringify("\\u0014Q"), '"\\\\u0014Q"');
assertEq(JSON.stringify("\\u0015Q"), '"\\\\u0015Q"');
assertEq(JSON.stringify("\\u0016Q"), '"\\\\u0016Q"');
assertEq(JSON.stringify("\\u0017Q"), '"\\\\u0017Q"');
assertEq(JSON.stringify("\\u0018Q"), '"\\\\u0018Q"');
assertEq(JSON.stringify("\\u0019Q"), '"\\\\u0019Q"');
assertEq(JSON.stringify("\\u001AQ"), '"\\\\u001AQ"');
assertEq(JSON.stringify("\\u001BQ"), '"\\\\u001BQ"');
assertEq(JSON.stringify("\\u001CQ"), '"\\\\u001CQ"');
assertEq(JSON.stringify("\\u001DQ"), '"\\\\u001DQ"');
assertEq(JSON.stringify("\\u001EQ"), '"\\\\u001EQ"');
assertEq(JSON.stringify("\\u001FQ"), '"\\\\u001FQ"');
assertEq(JSON.stringify("\\u0020Q"), '"\\\\u0020Q"');

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
