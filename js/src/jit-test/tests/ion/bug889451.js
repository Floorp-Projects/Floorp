/*
js> (((-1 >>> 1) + 1) * Math.pow(2, 52 - 30) + 1) & 1         
0
js> (((-1 >> 1) + 1) * Math.pow(2, 52 - 30) + 1) & 1 
1
*/
 
function f(x) {
  if (x >= 0) {
    // if it does not fail, try with lower power of 2.
    return (((x >>> 1) + 1) * 4194304 /* 2 ** (52 - 30) */ + 1) & 1;
  }
  return 2;
}
 
assertEq(f(-1 >>> 1), 1);
assertEq(f(-1 >>> 0), 0);
assertEq(f(-1 >>> 0), 0);
