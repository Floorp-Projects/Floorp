// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

// See bug 627984, comment 17, item 2.
var obj = {};
var x;
obj.watch("m", function (id, oldval, newval) {
        x = this.m;
        return newval;
    });
delete obj.m;
obj.m = function () { return this.method; };
obj.m = 2;

reportCompare(0, 0, 'ok');
