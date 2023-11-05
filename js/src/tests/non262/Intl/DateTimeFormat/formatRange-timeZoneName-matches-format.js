// |reftest| skip-if(!this.hasOwnProperty("Intl"))

// Test DateTimeFormat.prototype.format and DateTimeFormat.prototype.formatRange
// use the same formatting for the "timeZoneName" option.

function hours(v) {
  return v * 60 * 60 * 1000;
}

const locales = [
  "en", "fr", "de", "es",
  "ja", "th", "zh", "ar",
];

const timeZones = [
  "UTC", "Etc/GMT-1", "Etc/GMT+1",
  "Africa/Cairo",
  "America/New_York", "America/Los_Angeles",
  "Asia/Riyadh", "Asia/Bangkok", "Asia/Shanghai", "Asia/Tokyo",
  "Europe/Berlin", "Europe/London", "Europe/Madrid", "Europe/Paris",
];

const timeZoneNames = [
  "short", "shortOffset", "shortGeneric",
  "long", "longOffset", "longGeneric",
];

function timeZonePart(parts) {
  return parts.filter(({type}) => type === "timeZoneName").map(({value}) => value)[0];
}

for (let locale of locales) {
  for (let timeZone of timeZones) {
    for (let timeZoneName of timeZoneNames) {
      let dtf = new Intl.DateTimeFormat(locale, {timeZone, timeZoneName, hour: "numeric"});

      let formatTimeZone = timeZonePart(dtf.formatToParts(0));
      let formatRangeTimeZone = timeZonePart(dtf.formatRangeToParts(0, hours(1)));
      assertEq(formatRangeTimeZone, formatTimeZone);
    }
  }
}

if (typeof reportCompare === "function")
  reportCompare(0, 0, "ok");
