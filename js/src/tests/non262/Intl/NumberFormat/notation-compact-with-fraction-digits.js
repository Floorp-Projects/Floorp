// |reftest| skip-if(!this.hasOwnProperty("Intl"))

let nf = new Intl.NumberFormat("en", {
  minimumFractionDigits: 0,
  maximumFractionDigits: 2,
  notation: "compact",
});

assertEq(nf.format(1500), "1.5K");
assertEq(nf.format(1520), "1.52K");
assertEq(nf.format(1540), "1.54K");
assertEq(nf.format(1550), "1.55K");
assertEq(nf.format(1560), "1.56K");
assertEq(nf.format(1580), "1.58K");

if (typeof reportCompare === "function")
  reportCompare(true, true);
