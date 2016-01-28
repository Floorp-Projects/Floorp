var ta = new Uint8Array(1);
ta[0] = 17;

assertEq(ta.lastIndexOf(17, -0), +0);

if (typeof reportCompare === "function")
  reportCompare(true, true);
