// |reftest| skip-if(!this.hasOwnProperty("Intl"))

const calendars = Intl.supportedValuesOf("calendar");

assertEq(new Set(calendars).size, calendars.length, "No duplicates are present");
assertEqArray(calendars, [...calendars].sort(), "The list is sorted");

const typeRE = /^[a-z0-9]{3,8}(-[a-z0-9]{3,8})*$/;
for (let calendar of calendars) {
  assertEq(typeRE.test(calendar), true, `${calendar} matches the 'type' production`);
}

for (let calendar of calendars) {
  assertEq(new Intl.Locale("und", {calendar}).calendar, calendar, `${calendar} is canonicalised`);
}

for (let calendar of calendars) {
  let obj = new Intl.DateTimeFormat("en", {calendar});
  assertEq(obj.resolvedOptions().calendar, calendar, `${calendar} is supported by DateTimeFormat`);
}

assertEq(calendars.includes("gregory"), true, `Includes the Gregorian calendar`);

if (typeof reportCompare === "function")
  reportCompare(true, true);
