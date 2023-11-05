// |reftest| skip-if(!this.hasOwnProperty("Intl"))

const {
  Hour, Minute, Literal, TimeZoneName,
} = DateTimeFormatParts;

const tests = {
  "en": [
    {
      date: 0,
      timeZone: "-23",
      options: {hour: "numeric", minute: "numeric", hour12: false},
      timeZoneNames: {
        short: [Hour("01"), Literal(":"), Minute("00"), Literal(" "), TimeZoneName("GMT-23")],
        shortOffset: "short",
        shortGeneric: "short",
        long: [Hour("01"), Literal(":"), Minute("00"), Literal(" "), TimeZoneName("GMT-23:00")],
        longOffset: "long",
        longGeneric: "long",
      },
    },
    {
      date: 0,
      timeZone: "+23:59",
      options: {hour: "numeric", minute: "numeric", hour12: false},
      timeZoneNames: {
        short: [Hour("23"), Literal(":"), Minute("59"), Literal(" "), TimeZoneName("GMT+23:59")],
        shortOffset: "short",
        shortGeneric: "short",
        long: [Hour("23"), Literal(":"), Minute("59"), Literal(" "), TimeZoneName("GMT+23:59")],
        longOffset: "long",
        longGeneric: "long",
      },
    },
  ],
  "fr": [
    {
      date: 0,
      timeZone: "-23",
      options: {hour: "numeric", minute: "numeric", hour12: false},
      timeZoneNames: {
        short: [Hour("01"), Literal(":"), Minute("00"), Literal(" "), TimeZoneName("UTC\u{2212}23")],
        shortOffset: "short",
        shortGeneric: "short",
        long: [Hour("01"), Literal(":"), Minute("00"), Literal(" "), TimeZoneName("UTC\u{2212}23:00")],
        longOffset: "long",
        longGeneric: "long",
      },
    },
    {
      date: 0,
      timeZone: "+23:59",
      options: {hour: "numeric", minute: "numeric", hour12: false},
      timeZoneNames: {
        short: [Hour("23"), Literal(":"), Minute("59"), Literal(" "), TimeZoneName("UTC+23:59")],
        shortOffset: "short",
        shortGeneric: "short",
        long: [Hour("23"), Literal(":"), Minute("59"), Literal(" "), TimeZoneName("UTC+23:59")],
        longOffset: "long",
        longGeneric: "long",
      },
    },
  ],
  "ar": [
    {
      date: 0,
      timeZone: "+13:37",
      options: {hour: "numeric", minute: "numeric", hour12: false},
      timeZoneNames: {
        short: [Hour("١٣"), Literal(":"), Minute("٣٧"), Literal(" "), TimeZoneName("غرينتش+١٣:٣٧")],
        shortOffset: "short",
        shortGeneric: "short",
        long: "short",
        longOffset: "short",
        longGeneric: "short",
      },
    },
  ],
  "zh": [
    {
      date: 0,
      timeZone: "+23:59",
      options: {hour: "numeric", minute: "numeric", hour12: false},
      timeZoneNames: {
        short: [TimeZoneName("GMT+23:59"), Literal(" "), Hour("23"), Literal(":"), Minute("59")],
        shortOffset: "short",
        shortGeneric: "short",
        long: "short",
        longOffset: "short",
        longGeneric: "short",
      },
    },
  ],
};

for (let [locale, formats] of Object.entries(tests)) {
  for (let {date, timeZone, options, timeZoneNames} of formats) {
    for (let [timeZoneName, format] of Object.entries(timeZoneNames)) {
      let df = new Intl.DateTimeFormat(locale, {timeZone, timeZoneName, ...options});
      if (typeof format === "string") {
        format = timeZoneNames[format];
      }
      assertParts(df, date, format);
    }
  }
}

if (typeof reportCompare === "function")
  reportCompare(0, 0, "ok");
