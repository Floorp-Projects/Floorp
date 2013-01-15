
Array.prototype[1] = 'y';
var a = [0, (8)];
for (var i in a) {
    delete a[1];
}
