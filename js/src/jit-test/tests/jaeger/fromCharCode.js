/*
* Any copyright is dedicated to the Public Domain.
* http://creativecommons.org/licenses/publicdomain/
*/

for (var i = 0; i <= 0xFFFF; i++) {
    assertEq(String.fromCharCode(i).charCodeAt(0), i);
}
