gczeal(0);
try {
  v2 = (x = []);
  g1 = newGlobal({ sameZoneAs: x });
  enableShellAllocationMetadataBuilder();
  g1.Uint8ClampedArray.prototype = b1;
} catch(e) {}
startgc(9);
recomputeWrappers();
