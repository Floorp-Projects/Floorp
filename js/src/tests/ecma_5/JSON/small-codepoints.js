// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

var gTestfile = 'small-codepoints.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 554079;
var summary = 'JSON.parse should reject U+0000 through U+001F';

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

for (var i = 0; i <= 0x1F; i++)
  testJSON('["a' + String.fromCharCode(i) + 'c"]', true);
