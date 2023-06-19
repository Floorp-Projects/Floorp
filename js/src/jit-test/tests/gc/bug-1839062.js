// |jit-test| --ion-offthread-compile=off; --baseline-eager
gczeal(4);
a = new BigInt64Array(2);
for (x=1;x<100;++x)
  a[0];
