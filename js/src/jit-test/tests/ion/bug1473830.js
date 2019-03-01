
y = [];
y.forEach(function() {});

x = [];
for (var i = 0; i < 100; ++i) {
    x.push(undefined, 1);
}
x.sort();
x.reverse();

x.forEach(function(j) {
    "use strict";
    assertEq(this, 4);
    if (j) {
        x.forEach(function(z) { });
    }
}, 4);