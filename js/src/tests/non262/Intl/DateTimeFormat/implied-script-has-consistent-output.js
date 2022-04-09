// |reftest| skip-if(!this.hasOwnProperty("Intl"))

if (typeof getAvailableLocalesOf === "undefined") {
  var getAvailableLocalesOf = SpecialPowers.Cu.getJSTestingFunctions().getAvailableLocalesOf;
}

// Retrieve all available locales of Intl.DateTimeFormat.
const available = getAvailableLocalesOf("DateTimeFormat");

const options = [
  // Include "hour" to catch hour cycle differences.
  //
  // For example "ff-Latn-GH" (Fulah as spoken in Ghana) uses a 12-hour clock,
  // whereas "ff" (Fulah) uses a 24-hour clock. When the user creates the
  // formatter for "ff-GH", it should use the same formatter data as "ff-Latn-GH"
  // and it shouldn't fallback to "ff".
  {hour: "2-digit", minute: "2-digit", timeZone: "UTC"},

  // Include "timeZoneName" to catch script differences, e.g traditional or
  // simplified Chinese characters.
  {timeZoneName: "long", timeZone: "America/Los_Angeles"},
];

// Pick a date after 12 pm to catch any hour cycle differences.
const date = Date.UTC(2021, 6-1, 7, 15, 0);

available.map(x => {
  return new Intl.Locale(x);
}).filter(loc => {
  // Find all locales which have both a script and a region subtag.
  return loc.script && loc.region;
}).forEach(loc => {
  // Remove the script subtag from the locale.
  let noScript = new Intl.Locale(`${loc.language}-${loc.region}`);

  // Add the likely script subtag.
  let maximized = noScript.maximize();

  for (let opt of options) {
    // Formatter for the locale without a script subtag.
    let df1 = new Intl.DateTimeFormat(noScript, options);

    // Formatter for the locale with the likely script subtag added.
    let df2 = new Intl.DateTimeFormat(maximized, options);

    // The output for the locale without a script subtag should match the output
    // with the likely script subtag added.
    assertEq(df1.format(date), df2.format(date), `Mismatch for locale "${noScript}"`);
  }
});

if (typeof reportCompare === "function")
  reportCompare(0, 0, "ok");
