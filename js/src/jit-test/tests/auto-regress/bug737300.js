// |jit-test| error:ReferenceError

// Binary: cache/js-dbg-64-b5b6e6aebb36-linux
// Flags: --ion-eager
//

function loopy(p0)
{
 var r3 = p0;
 var r4 = p0;
 var r6 = p0;
 var r7 = p0;
 while (r2) {
  while (r2) {
   r5 = r6 & r1;
   r3 = r4 & r3;
   r2 = r7;
  }
 }
}
loopy(0);
