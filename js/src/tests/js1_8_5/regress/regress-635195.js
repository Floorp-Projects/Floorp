// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

var obj = {set x(v) {}};
obj.watch("x", function() { delete obj.x; });
obj.x = "hi";  // don't assert

reportCompare(0, 0, 'ok');
