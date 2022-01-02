var arr = new Uint8ClampedArray(10*1024*1024);
var sum = 0;
for (var i = 0; i < 10000; i++)
    sum += arr[i];
