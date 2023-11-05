// |reftest| skip-if(!this.hasOwnProperty("Intl"))

// Test all "timeZoneName" options with various locales when formatting a
// date-time range.

const {
  Hour, Minute, Literal, TimeZoneName,
} = DateTimeFormatParts;

function hours(v) {
  return v * 60 * 60 * 1000;
}

function minutes(v) {
  return v * 60 * 1000;
}

const tests = {
  "en": [
    {
      start: 0,
      end: minutes(2),
      timeZone: "UTC",
      options: {hour: "numeric", minute: "numeric", hour12: false},
      timeZoneNames: {
        short: [Hour("00"), Literal(":"), Minute("00"), Literal(" – "), Hour("00"), Literal(":"), Minute("02"), Literal(" "), TimeZoneName("UTC")],
        shortOffset: [Hour("00"), Literal(":"), Minute("00"), Literal(" – "), Hour("00"), Literal(":"), Minute("02"), Literal(" "), TimeZoneName("GMT")],
        shortGeneric: "shortOffset",
        long: [Hour("00"), Literal(":"), Minute("00"), Literal(" – "), Hour("00"), Literal(":"), Minute("02"), Literal(" "), TimeZoneName("Coordinated Universal Time")],
        longOffset: "shortOffset",
        longGeneric: "shortOffset",
      },
    },
    {
      start: 0,
      end: minutes(2),
      timeZone: "America/New_York",
      options: {hour: "numeric", minute: "numeric", hour12: false},
      timeZoneNames: {
        short: [Hour("19"), Literal(":"), Minute("00"), Literal(" – "), Hour("19"), Literal(":"), Minute("02"), Literal(" "), TimeZoneName("EST")],
        shortOffset: [Hour("19"), Literal(":"), Minute("00"), Literal(" – "), Hour("19"), Literal(":"), Minute("02"), Literal(" "), TimeZoneName("GMT-5")],
        shortGeneric: [Hour("19"), Literal(":"), Minute("00"), Literal(" – "), Hour("19"), Literal(":"), Minute("02"), Literal(" "), TimeZoneName("ET")],
        long: [Hour("19"), Literal(":"), Minute("00"), Literal(" – "), Hour("19"), Literal(":"), Minute("02"), Literal(" "), TimeZoneName("Eastern Standard Time")],
        longOffset: [Hour("19"), Literal(":"), Minute("00"), Literal(" – "), Hour("19"), Literal(":"), Minute("02"), Literal(" "), TimeZoneName("GMT-05:00")],
        longGeneric: [Hour("19"), Literal(":"), Minute("00"), Literal(" – "), Hour("19"), Literal(":"), Minute("02"), Literal(" "), TimeZoneName("Eastern Time")],
      },
    },
  ],
  "fr": [
    {
      start: 0,
      end: hours(2),
      timeZone: "UTC",
      options: {hour: "numeric", hour12: false},
      timeZoneNames: {
        short: [Hour("00"), Literal(" – "), Hour("02"), Literal(" "), TimeZoneName("UTC")],
        shortOffset: "short",
        shortGeneric: "short",
        long: [Hour("00"), Literal(" – "), Hour("02"), Literal(" "), TimeZoneName("temps universel coordonné")],
        longOffset: "short",
        longGeneric: "short",
      },
    },
    {
      start: minutes(15),
      end: hours(5) + minutes(30),
      timeZone: "Europe/Paris",
      options: {hour: "numeric", minute: "numeric", hour12: false},
      timeZoneNames: {
        short: [Hour("01"), Literal(":"), Minute("15"), Literal(" – "), Hour("06"), Literal(":"), Minute("30"), Literal(" "), TimeZoneName("UTC+1")],
        shortOffset: "short",
        shortGeneric: [Hour("01"), Literal(":"), Minute("15"), Literal(" – "), Hour("06"), Literal(":"), Minute("30"), Literal(" "), TimeZoneName("heure : France")],
        long: [Hour("01"), Literal(":"), Minute("15"), Literal(" – "), Hour("06"), Literal(":"), Minute("30"), Literal(" "), TimeZoneName("heure normale d’Europe centrale")],
        longOffset: [Hour("01"), Literal(":"), Minute("15"), Literal(" – "), Hour("06"), Literal(":"), Minute("30"), Literal(" "), TimeZoneName("UTC+01:00")],
        longGeneric: "long",
      },
    },
  ],
  "de": [
    {
      start: 0,
      end: hours(2),
      timeZone: "UTC",
      options: {hour: "numeric", hour12: false},
      timeZoneNames: {
        short: [Hour("00"), Literal("–"), Hour("02"), Literal(" Uhr "), TimeZoneName("UTC")],
        shortOffset: [Hour("00"), Literal("–"), Hour("02"), Literal(" Uhr "), TimeZoneName("GMT")],
        shortGeneric: "shortOffset",
        long: [Hour("00"), Literal("–"), Hour("02"), Literal(" Uhr "), TimeZoneName("Koordinierte Weltzeit")],
        longOffset: "shortOffset",
        longGeneric: "shortOffset",
      },
    },
    {
      start: hours(5) + minutes(15),
      end: hours(5) + minutes(30),
      timeZone: "Europe/Berlin",
      options: {hour: "numeric", minute: "numeric", hour12: false},
      timeZoneNames: {
        short: [Hour("06"), Literal(":"), Minute("15"), Literal("–"), Hour("06"), Literal(":"), Minute("30"), Literal(" Uhr "), TimeZoneName("MEZ")],
        shortOffset: [Hour("06"), Literal(":"), Minute("15"), Literal("–"), Hour("06"), Literal(":"), Minute("30"), Literal(" Uhr "), TimeZoneName("GMT+1")],
        shortGeneric: "short",
        long: [Hour("06"), Literal(":"), Minute("15"), Literal("–"), Hour("06"), Literal(":"), Minute("30"), Literal(" Uhr "), TimeZoneName("Mitteleuropäische Normalzeit")],
        longOffset: [Hour("06"), Literal(":"), Minute("15"), Literal("–"), Hour("06"), Literal(":"), Minute("30"), Literal(" Uhr "), TimeZoneName("GMT+01:00")],
        longGeneric: "long",
      },
    },
  ],
};

for (let [locale, formats] of Object.entries(tests)) {
  for (let {start, end, timeZone, options, timeZoneNames} of formats) {
    for (let [timeZoneName, format] of Object.entries(timeZoneNames)) {
      let df = new Intl.DateTimeFormat(locale, {timeZone, timeZoneName, ...options});
      if (typeof format === "string") {
        format = timeZoneNames[format];
      }
      assertRangeParts(df, start, end, format);
    }
  }
}

if (typeof reportCompare === "function")
  reportCompare(0, 0, "ok");
