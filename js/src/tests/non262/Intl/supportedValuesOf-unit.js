// |reftest| skip-if(!this.hasOwnProperty("Intl"))

const units = Intl.supportedValuesOf("unit");

assertEq(new Set(units).size, units.length, "No duplicates are present");
assertEqArray(units, [...units].sort(), "The list is sorted");

const unitRE = /^[a-z]+(-[a-z]+)*$/;
for (let unit of units) {
  assertEq(unitRE.test(unit), true, `${unit} is ASCII lower-case, separated by hyphens`);
  assertEq(unit.includes("-per-"), false, `${unit} is a simple unit identifier`);
}

for (let unit of units) {
  let obj = new Intl.NumberFormat("en", {style: "unit", unit});
  assertEq(obj.resolvedOptions().unit, unit, `${unit} is supported by NumberFormat`);
}

if (typeof reportCompare === "function")
  reportCompare(true, true);
