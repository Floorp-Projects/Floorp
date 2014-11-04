// Binary: cache/js-dbg-64-4fddb9923ef0-linux
// Flags: --ion-eager
//

function printBugNumber (num) {
  BUGNUMBER = num;
}
gcslice(1)
schedulegc(this);
gcslice(2);
var BUGNUMBER = ("one");
printBugNumber();
