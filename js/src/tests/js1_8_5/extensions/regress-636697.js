// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

var a = {set p(x) {}};
a.watch('p', function () {});
var b = Object.create(a);
b.watch('p', function () {});
delete b.p;
b.p = 0;

reportCompare(0, 0, 'ok');
