// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

// See bug 627984, comment 17, item 3.
var obj = {};
obj.watch("m", function (id, oldval, newval) {
        delete obj.m;
        obj.m = function () {};
        return newval;
    });
delete obj.m;
obj.m = 1;
assertEq(obj.m, 1);

reportCompare(0, 0, 'ok');
