// |reftest| skip-if(!this.hasOwnProperty("Intl"))

const timeZones = Intl.supportedValuesOf("timeZone");

assertEq(new Set(timeZones).size, timeZones.length, "No duplicates are present");
assertEqArray(timeZones, [...timeZones].sort(), "The list is sorted");

// The pattern doesn't cover the complete time zone syntax, but gives a good first approximation.
const timeZoneRE = /^[a-z0-9_+-]+(\/[a-z0-9_+-]+)*$/i;
for (let timeZone of timeZones) {
  assertEq(timeZoneRE.test(timeZone), true, `${timeZone} is ASCII`);
}

for (let timeZone of timeZones) {
  let obj = new Intl.DateTimeFormat("en", {timeZone});
  assertEq(obj.resolvedOptions().timeZone, timeZone, `${timeZone} is supported by DateTimeFormat`);
}

if (typeof reportCompare === "function")
  reportCompare(true, true);
