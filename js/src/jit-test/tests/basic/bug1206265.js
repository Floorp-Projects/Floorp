x = [
    objectEmulatingUndefined(),
    function() {}
];
x.forEach(function() {});
this.x.sort(function() {});
assertEq(x[0] instanceof Function, false);
assertEq(x[1] instanceof Function, true);
