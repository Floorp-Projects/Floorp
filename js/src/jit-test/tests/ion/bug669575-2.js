function loopy(p0)
{
 var r1 = p0;
 var r2 = p0;
 var r3 = p0;
 var r4 = p0;
 var r5 = p0;
 var r6 = p0;
 var r7 = p0;

 while (r2) {
  while (r2) {
   r1 = r4;
   r5 = r6 & r1;
   r3 = r4 & r3;
  }
  while (r2) {
   r6 = r2;
   r3 = r7;
  }
 }

 return 0;
}
loopy(0);


