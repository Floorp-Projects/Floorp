// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/
// Contributor: Luke Wagner <lw@mozilla.com>

var x, f;
for (var i = 0; i < 100; i++) {
    f = function() {};
    f.foo;
    x = f.length;
}
