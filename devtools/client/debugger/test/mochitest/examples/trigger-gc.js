setTimeout(() => {
  console.log("GC Triggered");
  SpecialPowers.gc();
}, 0);
