// |reftest| skip-if(!this.hasOwnProperty("Intl"))

const numSystems = Intl.supportedValuesOf("numberingSystem");

assertEq(new Set(numSystems).size, numSystems.length, "No duplicates are present");
assertEqArray(numSystems, [...numSystems].sort(), "The list is sorted");

const typeRE = /^[a-z0-9]{3,8}(-[a-z0-9]{3,8})*$/;
for (let numberingSystem of numSystems) {
  assertEq(typeRE.test(numberingSystem), true, `${numberingSystem} matches the 'type' production`);
}

for (let numberingSystem of numSystems) {
  assertEq(new Intl.Locale("und", {numberingSystem}).numberingSystem, numberingSystem,
           `${numberingSystem} is canonicalised`);
}

for (let numberingSystem of numSystems) {
  let obj = new Intl.DateTimeFormat("en", {numberingSystem});
  assertEq(obj.resolvedOptions().numberingSystem, numberingSystem,
           `${numberingSystem} is supported by DateTimeFormat`);
}

for (let numberingSystem of numSystems) {
  let obj = new Intl.NumberFormat("en", {numberingSystem});
  assertEq(obj.resolvedOptions().numberingSystem, numberingSystem,
           `${numberingSystem} is supported by NumberFormat`);
}

for (let numberingSystem of numSystems) {
  let obj = new Intl.RelativeTimeFormat("en", {numberingSystem});
  assertEq(obj.resolvedOptions().numberingSystem, numberingSystem,
           `${numberingSystem} is supported by RelativeTimeFormat`);
}

if (typeof reportCompare === "function")
  reportCompare(true, true);
