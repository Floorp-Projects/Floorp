var re = /(?<x>a)|b/;

function f(j) {
    var s = String.fromCharCode(0x61 + (j === 1));
    var e = re.exec(s);
    if (e.groups.x !== "a") print(i,j);
}
for (var i = 0; i < 2; ++i) f(i);
