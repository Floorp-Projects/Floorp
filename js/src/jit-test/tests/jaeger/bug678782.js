var i = -1; var j = -1; var s = ''; var f = '';
var buf = serialize(new Date(NaN));
var a = [1/0, -1/0, 8.64e15 + 1, -(8.64e15 + 1)];
for (var i = 0; i < a.length; i++) {
    var n = a[i];
    var nbuf = serialize(n);
    for (var Number ; j < 8; j++)
        buf[j + 8] = nbuf[j];
}
