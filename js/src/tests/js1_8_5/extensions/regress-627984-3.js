// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

// Don't write string value to method slot.
// See bug 627984, comment 17, item 2.
var obj = {};
obj.watch("m", function (id, oldval, newval) {
        return 'ok';
    });
delete obj.m;
obj.m = function () { return this.x; };
assertEq(obj.m, 'ok');

reportCompare(0, 0, 'ok');
