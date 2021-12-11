const MAX = 10000;

var arr = [];
var str = "";
for (var i = 0; i < MAX; ++i) {
    /x/.test(str);
    str = str + 'xxxxxxxxxxxxxx';
    arr.push(str);
}

arr.length = Math.floor(MAX/3);
gc();

for (var i = 0; i < MAX; ++i) {
    /x/.test(arr[i]);
}
