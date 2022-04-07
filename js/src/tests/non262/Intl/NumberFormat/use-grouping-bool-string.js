// |reftest| skip-if(!this.hasOwnProperty("Intl"))

let nf1 = new Intl.NumberFormat("en", {useGrouping: "true"});
assertEq(nf1.format(1000), "1,000");

let nf2 = new Intl.NumberFormat("en", {useGrouping: "false"});
assertEq(nf2.format(1000), "1,000");

if (typeof reportCompare === "function")
  reportCompare(true, true);
