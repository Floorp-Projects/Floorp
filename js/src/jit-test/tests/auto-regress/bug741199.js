// Binary: cache/js-dbg-32-e96d5b1f47b8-linux
// Flags: --ion-eager
//

var a = new Array(1000 * 100);
var i = a.length;
while (i-- != 0) {}
gc();
(({ }).break--);
