// for-of on an empty slow array does nothing.

var a = [];
a.slow = true;
for (var x of a)
    fail();
