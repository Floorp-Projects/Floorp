var x = new ArrayBuffer(2);
gczeal(4);
actual = [].concat(x).toString();
var count2 = countHeap();
