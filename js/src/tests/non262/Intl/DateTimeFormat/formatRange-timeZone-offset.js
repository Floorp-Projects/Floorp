// |reftest| skip-if(!this.hasOwnProperty("Intl"))

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
      timeZone: "+00",
      options: {hour: "numeric", minute: "numeric", hour12: false},
      timeZoneNames: {
        short: [Hour("00"), Literal(":"), Minute("00"), Literal(" – "), Hour("00"), Literal(":"), Minute("02"), Literal(" "), TimeZoneName("GMT")],
        shortOffset: "short",
        shortGeneric: "short",
        long: [Hour("00"), Literal(":"), Minute("00"), Literal(" – "), Hour("00"), Literal(":"), Minute("02"), Literal(" "), TimeZoneName("Greenwich Mean Time")],
        longOffset: "short",
        longGeneric: "long",
      },
    },
    {
      start: 0,
      end: minutes(2),
      timeZone: "-20:01",
      options: {hour: "numeric", minute: "numeric", hour12: false},
      timeZoneNames: {
        short: [Hour("03"), Literal(":"), Minute("59"), Literal(" – "), Hour("04"), Literal(":"), Minute("01"), Literal(" "), TimeZoneName("GMT-20:01")],
        shortOffset: "short",
        shortGeneric: "short",
        long: "short",
        longOffset: "short",
        longGeneric: "short",
      },
    },
  ],
  "fr": [
    {
      start: 0,
      end: hours(2),
      timeZone: "+17",
      options: {hour: "numeric", hour12: false},
      timeZoneNames: {
        short: [Hour("17"), Literal(" – "), Hour("19"), Literal(" "), TimeZoneName("UTC+17")],
        shortOffset: "short",
        shortGeneric: "short",
        long: [Hour("17"), Literal(" – "), Hour("19"), Literal(" "), TimeZoneName("UTC+17:00")],
        longOffset: "long",
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
